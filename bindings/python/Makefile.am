if USE_PYTHON

SUBDIRS = .

AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(srcdir)/babeltrace/

# Since the shared object used by the python bindings is not built with libtool
# we need to manually set the `rpath` during linkage
AM_LDFLAGS=-L$(top_builddir)/formats/ctf/.libs -L$(top_builddir)/lib/.libs -Wl,-rpath,$(prefix)/lib

INSTALLED_FILES=installed_files.txt

all-local:
	CC="$(CC)" CFLAGS="$(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(GLIB_CFLAGS) $(AM_CFLAGS)" CPPFLAGS="$(AM_CPPFLAGS) $(CPPFLAGS)" LDFLAGS="$(AM_LDFLAGS) $(LDFLAGS)" $(PYTHON) $(builddir)/setup.py build_ext --swig-opts="-Wall -I$(top_srcdir)"
	$(PYTHON) $(builddir)/setup.py build

# This target installs the Python package and saves the path of all the
# installed files in the INSTALLED_FILES text file to be used during this
# uninstallation
install-exec-local:
	@opts="--prefix=$(prefix) --verbose --record $(INSTALLED_FILES) --no-compile $(DISTSETUPOPTS)"; \
	if [ "$(DESTDIR)" != "" ]; then \
		opts="$$opts --root=$(DESTDIR)"; \
	fi; \
	$(PYTHON) $(builddir)/setup.py install $$opts;

clean-local:
	rm -rf $(builddir)/build

# We use the INSTALLED_FILES file to clean up the install folder and delete
# the folder it self.
uninstall-local:
	cat $(INSTALLED_FILES) | xargs rm -rf
	cat $(INSTALLED_FILES) | $(GREP) "__init__.py" | xargs dirname | xargs rmdir
	rm -f $(INSTALLED_FILES)

CLEANFILES = babeltrace/babeltrace_wrap.c
endif
