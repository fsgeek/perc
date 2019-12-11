# include $(top_srcdir)/mk/c.am

# SUBDIRS = .

AM_CFLAGS = -Werror -Wall -O3 $(LIBNDCTL_CFLAGS)

dt_progs = daxtest
noinst_PROGRAMS = $(dt_progs)

TESTS = $(dt_progs)

daxtest_SOURCES  = daxtest.c

LDADD = -lpthread
# LDADD += -lrt
# LDADD += -ldl
LDADD += -lndctl
LDADD += -luuid

CLEANFILES = *~
