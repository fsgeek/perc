# include $(top_srcdir)/mk/c.am

SUBDIRS = .

cache_progs = perc_progs
noinst_PROGRAMS = $(cache_progs)

TESTS = $(cache_progs)

perc_progs_SOURCES  = cache_progs.c
perc_progs_SOURCES += cache.c

# LDADD  = -lrt
# LDADD += -ldl

CLEANFILES = *~
