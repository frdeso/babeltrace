if USE_PYTHON

SUBDIRS = .

AM_LDFLAGS=-L$(top_builddir)/formats/ctf/.libs -L$(top_builddir)/lib/.libs
AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(srcdir)/babeltrace/

INSTALLED_FILES=installed_files.txt

all-local:
	echo $(DEFAULT_INCLUDES)
	CC="$(CC)" CFLAGS="$(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(GLIB_CFLAGS) $(AM_CFLAGS)" CPPFLAGS="$(AM_CPPFLAGS) $(CPPFLAGS)" LDFLAGS="$(AM_LDFLAGS) $(LDFLAGS)" $(PYTHON) $(builddir)/setup.py build_ext --swig-opts="-Wall -I$(top_srcdir)"
	$(PYTHON) $(builddir)/setup.py build

install-exec-local:
	@opts="--prefix=$(prefix) --verbose --record $(INSTALLED_FILES) --no-compile $(DISTSETUPOPTS)"; \
	if [ "$(DESTDIR)" != "" ]; then \
		opts="$$opts --root=$(DESTDIR)"; \
	fi; \
	$(PYTHON) $(builddir)/setup.py install $$opts;

clean-local:
	rm -rf $(builddir)/build

uninstall-local:
	cat $(INSTALLED_FILES) | xargs rm -rf
	cat $(INSTALLED_FILES) | grep "__init__.py" | xargs dirname | xargs rmdir

CLEANFILES = babeltrace/babeltrace_wrap.c
endif
