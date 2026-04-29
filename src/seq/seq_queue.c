/*************************************************************************\
Copyright (c) 2010-2015 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
Copyright (c) 2026      ITER Organization
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
#include "seq.h"
#include "seq_debug.h"
#include "epicsVersion.h"

/*
 * Use epicsAtomic if available (EPICS >= 3.15)
 */
#ifndef VERSION_INT
#  define VERSION_INT(V,R,M,P) ( ((V)<<24) | ((R)<<16) | ((M)<<8) | (P))
#endif
#ifndef EPICS_VERSION_INT
#  define EPICS_VERSION_INT VERSION_INT(EPICS_VERSION, EPICS_REVISION, EPICS_MODIFICATION, EPICS_PATCH_LEVEL)
#endif

#if EPICS_VERSION_INT >= VERSION_INT(3,15,0,0)
#include "epicsAtomic.h"
#define HAS_ATOMICS
#endif

/* Fallbacks for older EPICS Base or when atomics are not available */
#ifndef HAS_ATOMICS
#define epicsAtomicGetSizeT(p) (*(p))
#define epicsAtomicSetSizeT(p,v) (*(p) = (v))
#define epicsAtomicGetIntT(p) (*(p))
#define epicsAtomicSetIntT(p,v) (*(p) = (v))
#define epicsAtomicReadMemoryBarrier()
#define epicsAtomicWriteMemoryBarrier()
#endif

struct seqQueue {
    size_t          wr;
    size_t          rd;
    size_t          numElems;
    size_t          elemSize;
    int             overflow;   /* Use int for atomic access */
    epicsMutexId    mutex;
    char            *buffer;
};

epicsShareFunc boolean seqQueueInvariant(QUEUE q)
{
    return (q != NULL)
        && q->elemSize > 0
        && q->numElems > 0
        && q->numElems <= seqQueueMaxNumElems
        && epicsAtomicGetSizeT(&q->rd) < q->numElems
        && epicsAtomicGetSizeT(&q->wr) < q->numElems;
}

epicsShareFunc QUEUE seqQueueCreate(size_t numElems, size_t elemSize)
{
    QUEUE q = new(struct seqQueue);

    if (!q) {
        errlogSevPrintf(errlogFatal, "seqQueueCreate: out of memory\n");
        return 0;
    }
    /* check arguments to establish invariants */
    if (numElems == 0) {
        errlogSevPrintf(errlogFatal, "seqQueueCreate: numElems must be positive\n");
        free(q);
        return 0;
    }
    if (elemSize == 0) {
        errlogSevPrintf(errlogFatal, "seqQueueCreate: elemSize must be positive\n");
        free(q);
        return 0;
    }
    if (numElems > seqQueueMaxNumElems) {
        errlogSevPrintf(errlogFatal, "seqQueueCreate: numElems too large\n");
        free(q);
        return 0;
    }
    DEBUG("%s:%d:calloc(%u,%u)\n",__FILE__,__LINE__,(unsigned)numElems, (unsigned)elemSize);
    q->buffer = (char *)calloc(numElems, elemSize);
    if (!q->buffer) {
        errlogSevPrintf(errlogFatal, "seqQueueCreate: out of memory\n");
        free(q);
        return 0;
    }
    q->mutex = epicsMutexCreate();
    if (!q->mutex) {
        errlogSevPrintf(errlogFatal, "seqQueueCreate: out of memory\n");
        free(q->buffer);
        free(q);
        return 0;
    }
    q->elemSize = elemSize;
    q->numElems = numElems;
    q->overflow = 0;
    q->rd = q->wr = 0;
    return q;
}

epicsShareFunc void seqQueueDestroy(QUEUE q)
{
    if (!q) return;
    epicsMutexDestroy(q->mutex);
    free(q->buffer);
    free(q);
}

epicsShareFunc boolean seqQueueGet(QUEUE q, void *value)
{
    return seqQueueGetF(q, memcpy, value);
}

epicsShareFunc boolean seqQueueGetF(QUEUE q, seqQueueFunc *get, void *arg)
{
#ifdef HAS_ATOMICS
    size_t rd = epicsAtomicGetSizeT(&q->rd);
    size_t wr = epicsAtomicGetSizeT(&q->wr);

    /* Lock-free fast path for Single-Consumer Get */
    if (wr != rd) {
        epicsAtomicReadMemoryBarrier();
        get(arg, q->buffer + rd * q->elemSize, q->elemSize);
        /* Ensure the data is read before we update the read index.
           This prevents a producer from overwriting the element before
           the consumer has finished copying it. */
        epicsAtomicWriteMemoryBarrier();
        epicsAtomicSetSizeT(&q->rd, (rd + 1) % q->numElems);
        return FALSE;
    }
#endif

    /* Mutex path for when wr == rd (empty or overflow)
       OR if we don't have atomics */
    epicsMutexLock(q->mutex);
    if (q->wr == q->rd) {
        if (!q->overflow) {
            epicsMutexUnlock(q->mutex);
            return TRUE;
        }
        get(arg, q->buffer + q->rd * q->elemSize, q->elemSize);
        /* check again, a put might have intervened */
        if (q->wr == q->rd && q->overflow) {
            epicsAtomicSetIntT(&q->overflow, 0);
        } else {
            epicsAtomicSetSizeT(&q->rd, (q->rd + 1) % q->numElems);
        }
    } else {
        /* Can happen if wr moved after our lock-free check */
        get(arg, q->buffer + q->rd * q->elemSize, q->elemSize);
        epicsAtomicSetSizeT(&q->rd, (q->rd + 1) % q->numElems);
    }
    epicsMutexUnlock(q->mutex);
    return FALSE;
}

epicsShareFunc boolean seqQueuePut(QUEUE q, const void *value)
{
    return seqQueuePutF(q, memcpy, value);
}

epicsShareFunc boolean seqQueuePutF(QUEUE q, seqQueueFunc *put, const void *arg)
{
    boolean r = FALSE;
    size_t rd, wr;

    /* Always use mutex for Put to support Multi-Producer and safely handle overflow. */
    epicsMutexLock(q->mutex);
    rd = epicsAtomicGetSizeT(&q->rd);
    wr = epicsAtomicGetSizeT(&q->wr);

    if (q->overflow || (wr + 1) % q->numElems == rd) {
        if ((wr + 1) % q->numElems == rd) {
            if (q->overflow) r = TRUE;
            epicsAtomicSetIntT(&q->overflow, 1);
        } else if (q->overflow) {
            /* A get happened, move wr forward */
            wr = (wr + 1) % q->numElems;
            epicsAtomicSetSizeT(&q->wr, wr);
            if ((wr + 1) % q->numElems != rd) {
                epicsAtomicSetIntT(&q->overflow, 0);
            }
        }
    }

    put(q->buffer + wr * q->elemSize, arg, q->elemSize);

    if (!epicsAtomicGetIntT(&q->overflow)) {
        epicsAtomicWriteMemoryBarrier();
        epicsAtomicSetSizeT(&q->wr, (wr + 1) % q->numElems);
    }
    epicsMutexUnlock(q->mutex);
    return r;
}

epicsShareFunc void seqQueueFlush(QUEUE q)
{
    epicsMutexLock(q->mutex);
    epicsAtomicSetSizeT(&q->rd, epicsAtomicGetSizeT(&q->wr));
    epicsAtomicSetIntT(&q->overflow, 0);
    epicsMutexUnlock(q->mutex);
}

static size_t used(const QUEUE q)
{
    size_t rd = epicsAtomicGetSizeT(&q->rd);
    size_t wr = epicsAtomicGetSizeT(&q->wr);
    return (q->numElems + wr - rd) % q->numElems + (epicsAtomicGetIntT(&q->overflow) ? 1 : 0);
}

epicsShareFunc size_t seqQueueFree(const QUEUE q)
{
    return q->numElems - used(q);
}

epicsShareFunc size_t seqQueueUsed(const QUEUE q)
{
    return used(q);
}

epicsShareFunc boolean seqQueueIsEmpty(const QUEUE q)
{
    size_t rd = epicsAtomicGetSizeT(&q->rd);
    size_t wr = epicsAtomicGetSizeT(&q->wr);
    return wr == rd && !epicsAtomicGetIntT(&q->overflow);
}

epicsShareFunc boolean seqQueueIsFull(const QUEUE q)
{
    size_t rd = epicsAtomicGetSizeT(&q->rd);
    size_t wr = epicsAtomicGetSizeT(&q->wr);
    return (wr + 1) % q->numElems == rd && epicsAtomicGetIntT(&q->overflow);
}

epicsShareFunc size_t seqQueueNumElems(const QUEUE q)
{
    return q->numElems;
}

epicsShareFunc size_t seqQueueElemSize(const QUEUE q)
{
    return q->elemSize;
}
