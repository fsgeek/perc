# include $(top_srcdir)/mk/c.am

SUBDIRS = .

AM_CFLAGS = -Werror -Wall # -g3 -O0

stream_progs = nvmalloc_progs
noinst_PROGRAMS = $(stream_progs)

TESTS = $(stream_progs)

nvmalloc_progs_SOURCES  = alloc_progs.c
nvmalloc_progs_SOURCES += cpu.c
#nvmalloc_progs_SOURCES += setprocessor.c

LDADD = -luuid
#LDADD += -lpthread
#LDADD += -lrt
#LDADD += -lnuma
#LDADD += -ldl

CLEANFILES = *~
