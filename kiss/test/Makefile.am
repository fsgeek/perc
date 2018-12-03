include $(top_srcdir)/mk/c.am

test_progs  = testkiss


noinst_PROGRAMS  = $(test_progs)

TESTS  = $(test_progs)

testkiss_SOURCES  = testkiss.c

LDADD  = -lrt
LDADD += $(top_builddir)/kiss/libkiss.la
LDADD += -ldl
LDADD += -luuid
LDADD += -lrt

testkiss_CPPFLAGS  = $(AM_CPPFLAGS)
testkiss_CPPFLAGS += -I$(top_srcdir)/libfuse/lib


CLEANFILES = *~
