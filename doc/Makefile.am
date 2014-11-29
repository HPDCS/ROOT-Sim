ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -I$(top_srcdir)/src/simulator -I$(top_srcdir)/src/simulator/subsystems
AM_MAKEFLAGS = --no-print-directory

if HAVE_DOXYGEN

EXTRA_DIST = html.timestamp customdoxygen.css doxy.conf footer.html header.html img/rootsim.png img/rs-arch-last.png
documentationdir = $(docdir)/
documentation_DATA = html.tar

html.tar: html

html: html.timestamp

html.timestamp:
	@doxygen doxy.conf > /dev/null 2>&1
	@touch $@
	@tar cvf html.tar html > /dev/null

clean-local:
	@rm -rf html html.timestamp html.tar

install-data-hook:
	@cd $(docdir); \
	tar xvf html.tar > /dev/null; \
	rm html.tar

uninstall-local:
	@cd $(docdir); \
	rm -rf html

endif
