include $(top_srcdir)/mk/c.am
include $(top_srcdir)/mk/rpcgen.am

SUBDIRS  = .
SUBDIRS += test

lib_LTLIBRARIES  = libkiss.la

#libkiss_la_includedir = $(includedir)
#libkiss_la_includedir += $(topdir)/include
#libkiss_la_includedir += ../include
#libkiss_include_HEADERS = kiss.h

libkiss_la_SOURCES = kiss_alloc.c
#libkiss_la_SOURCES += nicopenclose.c

libkiss_la_CPPFLAGS  = $(AM_CPPFLAGS)
libkiss_la_CPPFLAGS += -I$(top_srcdir)/include


libkiss_la_LIBADD  = -lpthread
libkiss_la_LIBADD += -lrt
libkiss_la_LIBADD += -luuid


CLEANFILES  = *~
CLEANFILES += $(BUILT_SOURCES) 

