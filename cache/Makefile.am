# include $(top_srcdir)/mk/c.am

SUBDIRS = .

AM_CFLAGS = -Werror -Wall -g3 -O0

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
