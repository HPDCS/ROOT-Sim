ACLOCAL_AMFLAGS = -I m4

if HAVE_DOXYGEN
directory = $(top_srcdir)/doc/html/

doxyfile.stamp:
	$(DOXYGEN) doxy.conf
	echo Timestamp > doxyfile.stamp

CLEANFILES = doxyfile.stamp

all-local: doxyfile.stamp
clean-local:
	rm -rf $(top_srcdir)/doc/html
endif
