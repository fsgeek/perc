# include $(top_srcdir)/mk/c.am

SUBDIRS = .
SUBDIRS += daxtest

AM_CFLAGS = -Werror -Wall -O3

cache_progs = perc_progs
noinst_PROGRAMS = $(cache_progs)

TESTS = $(cache_progs)

perc_progs_SOURCES  = cache_progs.c
perc_progs_SOURCES += cache.c
perc_progs_SOURCES += cpu.c
perc_progs_SOURCES += setprocessor.c

LDADD = -lpthread
# LDADD += -lrt
# LDADD += -ldl

CLEANFILES = *~
