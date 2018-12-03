include $(top_srcdir)/mk/c.am
include $(top_srcdir)/mk/rpcgen.am

SUBDIRS  = .
#KSUBDIRS += test

lib_LTLIBRARIES  = libcpu.la

libcpu_includedir = $(includedir)
libcpu_includedir += $(topdir)/include
#libcpu_include_HEADERS = cpu.h

libcpu_la_SOURCES  = cpu.c
libcpu_la_SOURCES += setprocessor.c

libcpu_la_CPPFLAGS  = -I$(top_srcdir)/include
#libcpu_la_CPPFLAGS += $(AM_CPPFLAGS)
libcpu_la_CFLAGS = -Werror -Wall

libcpu_la_LIBADD  = -lpthread
libcpu_la_LIBADD += -lrt


CLEANFILES  = *~

