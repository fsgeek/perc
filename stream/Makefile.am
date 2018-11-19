# include $(top_srcdir)/mk/c.am

SUBDIRS = .

AM_CFLAGS = -Werror -Wall # -g3 -O0

stream_progs = copy_progs
noinst_PROGRAMS = $(stream_progs)

TESTS = $(stream_progs)

copy_progs_SOURCES  = stream_progs.c
copy_progs_SOURCES += cache.c
copy_progs_SOURCES += cpu.c
copy_progs_SOURCES += setprocessor.c

LDADD = -lpthread
LDADD += -lrt
LDADD += -lnuma
# LDADD += -ldl

CLEANFILES = *~
