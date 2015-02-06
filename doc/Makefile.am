if HAVE_DOXYGEN
directory = $(top_srcdir)/docs/man/man3/

doxyfile.stamp:
	$(DOXYGEN) doxy.conf
	echo Timestamp > doxyfile.stamp

CLEANFILES = doxyfile.stamp

all-local: doxyfile.stamp
clean-local:
	rm -rf $(top_srcdir)/docs/man
endif
