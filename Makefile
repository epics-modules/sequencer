TOP = .
include $(TOP)/configure/CONFIG

DIRS += configure

DIRS += src
src_DEPEND_DIRS  = configure

DIRS += test
test_DEPEND_DIRS = src

DIRS += examples
examples_DEPEND_DIRS = src

include $(TOP)/configure/RULES_TOP

html: src
	$(MAKE) -C documentation

docs: src
	$(MAKE) -C documentation pdf=1

docs.clean:
	$(MAKE) -C documentation clean

realclean clean: docs.clean

.PHONY: html docs docs.clean
