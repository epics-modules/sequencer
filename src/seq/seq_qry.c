/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990-1994, The Regents of the University of California.
		         Los Alamos National Laboratory

	Copyright, 2010, Helmholtz-Zentrum Berlin f. Materialien
		und Energie GmbH, Germany (HZB)
		(see file Copyright.HZB included in this distribution)

	Task query & debug routines for run-time sequencer
***************************************************************************/

#include <string.h>

#include "seq.h"

static int wait_rtn(void);
static void printValue(void *pVal, int count, int type);
static SPROG *seqQryFind(epicsThreadId tid);
static void seqShowAll(void);

/*
 * seqShow() - Query the sequencer for state information.
 * If a non-zero thread id is specified then print the information about
 * the state program, otherwise print a brief summary of all state programs
 */
long epicsShareAPI seqShow(epicsThreadId tid)
{
	SPROG	*pSP;
	SSCB	*pSS;
	STATE	*pST;
	int	nss;
	double	timeNow, timeElapsed;

	pSP = seqQryFind(tid);
	if (pSP == NULL)
		return 0;

	/* Print info about state program */
	printf("State Program: \"%s\"\n", pSP->pProgName);
	printf("  initial thread id = %p\n", pSP->threadId);
	printf("  thread priority = %d\n", pSP->threadPriority);
	printf("  number of state sets = %ld\n", pSP->numSS);
	printf("  number of syncQ queues = %d\n", pSP->numQueues);
	if (pSP->numQueues > 0)
		printf("  queue array address = %p\n",pSP->pQueues);
	printf("  number of channels = %ld\n", pSP->numChans);
	printf("  number of channels assigned = %ld\n", pSP->assignCount);
	printf("  number of channels connected = %ld\n", pSP->connCount);
	printf("  options: async=%d, debug=%d, newef=%d, reent=%d, conn=%d, "
		"main=%d\n",
	 ((pSP->options & OPT_ASYNC) != 0), ((pSP->options & OPT_DEBUG) != 0),
	 ((pSP->options & OPT_NEWEF) != 0), ((pSP->options & OPT_REENT) != 0),
	 ((pSP->options & OPT_CONN)  != 0), ((pSP->options & OPT_MAIN)  != 0));
	if ((pSP->options & OPT_REENT) != 0)
		printf("  user variables: address = %lu = 0x%lx, length = %ld "
			"= 0x%lx bytes\n", (unsigned long)pSP->pVar,
			(unsigned long)pSP->pVar, pSP->varSize, pSP->varSize);
	if (pSP->logFd)
	{
		printf("  log file fd = %d\n", fileno(pSP->logFd));
		printf("  log file name = \"%s\"\n", pSP->pLogFile);
	}
	printf("\n");

	/* Print state set info */
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		int n;

		printf("  State Set: \"%s\"\n", pSS->pSSName);

		if (pSS->threadId != (epicsThreadId)0)
		{
			char threadName[THREAD_NAME_SIZE];
			epicsThreadGetName(pSS->threadId, threadName,sizeof(threadName));
			printf("  thread name = %s;", threadName);
		}

		printf("  thread id = %lu = 0x%lx\n", 
			(unsigned long) pSS->threadId, (unsigned long) pSS->threadId);

		pST = pSS->pStates;
		printf("  First state = \"%s\"\n", pST->pStateName);

		pST = pSS->pStates + pSS->currentState;
		printf("  Current state = \"%s\"\n", pST->pStateName);

		pST = pSS->pStates + pSS->prevState;
		printf("  Previous state = \"%s\"\n", pSS->prevState >= 0 ?
			pST->pStateName : "");

		pvTimeGetCurrentDouble(&timeNow);
		timeElapsed = timeNow - pSS->timeEntered;
		printf("  Elapsed time since state was entered = %.1f "
			"seconds\n", timeElapsed);
		printf("  Queued time delays:\n");
		for (n = 0; n < pSS->numDelays; n++)
		{
			printf("\tdelay[%2d]=%f", n, pSS->delay[n]);
			if (pSS->delayExpired[n])
				printf(" - expired");
			printf("\n");
		}
		printf("\n");
	}

	return 0;
}
/*
 * seqChanShow() - Show channel information for a state program.
 */
long epicsShareAPI seqChanShow(epicsThreadId tid, char *pStr)
{
	SPROG	*pSP;
	CHAN	*pDB;
	int	nch, n;
	char	tsBfr[50], connQual;
	int	match, showAll;

	pSP = seqQryFind(tid);
	if(!pSP) return 0;

	printf("State Program: \"%s\"\n", pSP->pProgName);
	printf("Number of channels=%ld\n", pSP->numChans);

	if (pStr != NULL)
	{
		connQual = pStr[0];
		/* Check for channel connect qualifier */
		if ((connQual == '+') || (connQual == '-'))
		{
			pStr += 1;
		}
	}
	else
		connQual = 0;

	pDB = pSP->pChan;
	for (nch = 0; nch < pSP->numChans; )
	{
		if (pStr != NULL)
		{
			/* Check for channel connect qualifier */
			if (connQual == '+')
				showAll = pDB->connected;
			else if (connQual == '-')
				showAll = pDB->assigned && (!pDB->connected);
			else
				showAll = TRUE;

			/* Check for pattern match if specified */
			match = (pStr[0] == 0) ||
					(strstr(pDB->dbName, pStr) != NULL);
			if (!(match && showAll))
			{
				pDB += 1;
				nch += 1;
				continue; /* skip this channel */
			}
		}
		printf("\n#%d of %ld:\n", nch+1, pSP->numChans);
		printf("Channel name: \"%s\"\n", pDB->dbName);
		printf("  Unexpanded (assigned) name: \"%s\"\n", pDB->dbAsName);
		printf("  Variable name: \"%s\"\n", pDB->pVarName);
		printf("    offset = %ld = 0x%lx\n", (long)pDB->offset, (long)pDB->offset);
		printf("    type = %s\n", pDB->pVarType);
		printf("    count = %ld\n", pDB->count);
		printValue(bufPtr(pDB)+pDB->offset, pDB->putType, pDB->count);

		printf("  Monitor flag = %d\n", pDB->monFlag);
		if (pDB->monitored)
			printf("    Monitored\n");
		else
			printf("    Not monitored\n");

		if (pDB->assigned)
			printf("  Assigned\n");
		else
			printf("  Not assigned\n");

		if(pDB->connected)
			printf("  Connected\n");
		else
			printf("  Not connected\n");

		if(pDB->getComplete)
			printf("  Last get completed\n");
		else
			printf("  Get not completed or no get issued\n");

		if(epicsEventTryWait(pDB->putSemId))
			printf("  Last put completed\n");
		else
			printf("  Put not completed or no put issued\n");

		printf("  Status = %d\n", pDB->status);
		printf("  Severity = %d\n", pDB->severity);
		printf("  Message = %s\n", pDB->message != NULL ?
			pDB->message : "");

		/* Print time stamp in text format: "yyyy/mm/dd hh:mm:ss.sss" */
		epicsTimeToStrftime(tsBfr, sizeof(tsBfr),
			"%Y/%m/%d %H:%M:%S.%03f", &pDB->timeStamp);
		printf("  Time stamp = %s\n", tsBfr);

		n = wait_rtn();
		if (n == 0)
			return 0;
		nch += n;
		if (nch < 0)
			nch = 0;
		pDB = pSP->pChan + nch;
	}

	return 0;
}
/*
 * seqcar() - Sequencer Channel Access Report
 */

struct seqStats
{
	int	level;
	int	nProgs;
	int	nChans;
	int	nConn;
};

static int seqcarCollect(SPROG *pSP, void *param)
{
	struct seqStats *pstats = (struct seqStats *) param;
	CHAN	*pDB = pSP->pChan;
	int	nch;
	int	level = pstats->level;
	int 	printedProgName = 0;
	pstats->nProgs++;
	for (nch = 0; nch < pSP->numChans; nch++)
	{
		if (pDB->assigned) pstats->nChans++;
		if (pDB->connected) pstats->nConn++;
		if (level > 1 ||
		    (level == 1 && !pDB->connected))
		    {
			if (!printedProgName)
			{
				printf("  Program \"%s\"\n", pSP->pProgName);
				printedProgName = 1;
			}
			printf("    Variable \"%s\" %sconnected to PV \"%s\"\n",
				pDB->pVarName,
				pDB->connected ? "" : "not ",
				pDB->dbName);
		}
		pDB++;
	}
	return FALSE;	/* continue traversal */
}

long epicsShareAPI seqcar(int level)
{
	struct seqStats stats = {0, 0, 0, 0};
	int diss;
	stats.level = level;
	seqTraverseProg(seqcarCollect, (void *) &stats);
	diss = stats.nChans - stats.nConn;
	printf("Total programs=%d, channels=%d, connected=%d, disconnected=%d\n",
		stats.nProgs, stats.nChans, stats.nConn, diss);
	return diss;
}

void epicsShareAPI seqcaStats(int *pchans, int *pdiscon)
{
	struct seqStats stats = {0, 0, 0, 0};
	seqTraverseProg(seqcarCollect, (void *) &stats);
	if (pchans)  *pchans  = stats.nChans;
	if (pdiscon) *pdiscon = stats.nChans - stats.nConn;
}
/*
 * seqQueueShow() - Show syncQ queue information for a state program.
 */
long epicsShareAPI seqQueueShow(epicsThreadId tid)
{
	SPROG	*pSP;
	ELLLIST	*pQueue;
	int	nque, n;
	char	tsBfr[50];

	pSP = seqQryFind(tid);
	if(!pSP) return 0;

	printf("State Program: \"%s\"\n", pSP->pProgName);
	printf("Number of queues = %d\n", pSP->numQueues);

	pQueue = pSP->pQueues;
	for (nque = 0; nque < pSP->numQueues; )
	{
		QENTRY	*pEntry;
		int i;

		printf("\nQueue #%d of %d:\n", nque+1, pSP->numQueues);
		printf("Number of entries = %d\n", ellCount(pQueue));
		for (pEntry = (QENTRY *) ellFirst(pQueue), i = 1;
		     pEntry != NULL;
		     pEntry = (QENTRY *) ellNext(&pEntry->node), i++)
		{
			CHAN	*pDB = pEntry->pDB;
			pvValue	*pAccess = &pEntry->value;
			void	*pVal = (char *)pAccess + pDB->dbOffset;

			printf("\nEntry #%d: channel name: \"%s\"\n",
							    i, pDB->dbName);
			printf("  Variable name: \"%s\"\n", pDB->pVarName);
			printValue(pVal, pDB->putType, 1);
							/* was pDB->count */
			printf("  Status = %d\n",
					pAccess->timeStringVal.status);
			printf("  Severity = %d\n",
					pAccess->timeStringVal.severity);

			/* Print time stamp in text format:
			   "yyyy/mm/dd hh:mm:ss.sss" */
			epicsTimeToStrftime(tsBfr, sizeof(tsBfr), "%Y/%m/%d "
				"%H:%M:%S.%03f", &pAccess->timeStringVal.stamp);
			printf("  Time stamp = %s\n", tsBfr);
		}

		n = wait_rtn();
		if (n == 0)
			return 0;
		nque += n;
		if (nque < 0)
			nque = 0;
		pQueue = pSP->pQueues + nque;
	}

	return 0;
}

/* Read from console until a RETURN is detected */
static int wait_rtn(void)
{
	char	bfr[10];
	int	i, n;

	printf("Next? (+/- skip count)\n");
	for (i = 0;  i < 10; i++)
	{
		int c = getchar ();
		if (c == EOF)
			break;
		if ((bfr[i] = c) == '\n')
			break;
	}
	bfr[i] = 0;
	if (bfr[0] == 'q')
		return 0; /* quit */

	n = atoi(bfr);
	if (n == 0)
		n = 1;
	return n;
}

/* Print the current internal value of a database channel */
static void printValue(void *pVal, int count, int type)
{
	int	i;
	char	*c;
	short	*s;
	long	*l;
	float	*f;
	double	*d;

	printf("  Value =");
	for (i = 0; i < count; i++)
	{
	  switch (type)
	  {
	    case pvTypeSTRING:
		c = (char *)pVal;
		for (i = 0; i < count; i++, c += sizeof(pvString))
		{
			printf(" %s", c);
		}
		break;

	     case pvTypeCHAR:
		c = (char *)pVal;
		for (i = 0; i < count; i++, c++)
		{
			printf(" %d", *c);
		}
		break;

	    case pvTypeSHORT:
		s = (short *)pVal;
		for (i = 0; i < count; i++, s++)
		{
			printf(" %d", *s);
		}
		break;

	    case pvTypeLONG:
		l = (long *)pVal;
		for (i = 0; i < count; i++, l++)
		{
			printf(" %ld", *l);
		}
		break;

	    case pvTypeFLOAT:
		f = (float *)pVal;
		for (i = 0; i < count; i++, f++)
		{
			printf(" %g", *f);
		}
		break;

	    case pvTypeDOUBLE:
		d = (double *)pVal;
		for (i = 0; i < count; i++, d++)
		{
			printf(" %g", *d);
		}
		break;
	  }
	}

	printf("\n");
}

/* Find a state program associated with a given thread id */
static SPROG *seqQryFind(epicsThreadId tid)
{
	SPROG	*pSP;

	if (tid == 0)
	{
		seqShowAll();
		return NULL;
	}

	/* Find a state program that has this thread id */
	pSP = seqFindProg(tid);
	if (pSP == NULL)
	{
		printf("No state program exists for thread id %ld\n", (long)tid);
		return NULL;
	}

	return pSP;
}

static int	seqProgCount;

/* This routine is called by seqTraverseProg() for seqShowAll() */
static int seqShowSP(SPROG *pSP, void *parg)
{
	SSCB	*pSS;
	int	nss;
	char	*progName;
	char	threadName[THREAD_NAME_SIZE];

	if (seqProgCount++ == 0)
		printf("Program Name     Thread ID  Thread Name      SS Name\n\n");

	progName = pSP->pProgName;
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		if (pSS->threadId == 0)
			strcpy(threadName,"(no thread)");
		else
			epicsThreadGetName(pSS->threadId, threadName,
				      sizeof(threadName));
		printf("%-16s %-10p %-16s %-16s\n", progName,
			pSS->threadId, threadName, pSS->pSSName );
		progName = "";
	}
	printf("\n");
	return FALSE;	/* continue traversal */
}

/* Print a brief summary of all state programs */
static void seqShowAll(void)
{

	seqProgCount = 0;
	seqTraverseProg(seqShowSP, 0);
	if (seqProgCount == 0)
		printf("No active state programs\n");
	return;
}
