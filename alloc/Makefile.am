# include $(top_srcdir)/mk/c.am

SUBDIRS = .

AM_CFLAGS = -Werror -Wall # -g3 -O0

stream_progs = nvmalloc_progs
noinst_PROGRAMS = $(stream_progs)

TESTS = $(stream_progs)

nvmalloc_progs_SOURCES  = alloc_progs.c

LDADD = -lpthread
LDADD += -lrt
LDADD += -lnuma
# LDADD += -ldl

CLEANFILES = *~
