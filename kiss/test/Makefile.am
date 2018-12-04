include $(top_srcdir)/mk/c.am

test_progs  = testkiss


noinst_PROGRAMS  = $(test_progs)

TESTS  = $(test_progs)

testkiss_SOURCES  = testkiss.c

LDADD  = $(top_builddir)/kiss/libkiss.la
LDADD += $(top_builddir)/munit/libmunit.la
LDADD += $(top_builddir)/cpu/libcpu.la
LDADD += -lrt
LDADD += -ldl
LDADD += -luuid
LDADD += -lrt

testkiss_CPPFLAGS  = $(AM_CPPFLAGS)
testkiss_CPPFLAGS += -I$(top_srcdir)/munit
testkiss_CPPFLAGS += -I$(top_srcdir)/kiss


CLEANFILES = *~
