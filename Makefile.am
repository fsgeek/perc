AUTOMAKE_OPTIONS = foreign
SUBDIRS = cache stream

ACLOCAL_AMFLAGS = -I m4 --install
dist_noinst_SCRIPTS = autogen.sh

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

CLEANFILES = *~
