ACLOCAL_AMFLAGS = -I m4 
AM_MAKEFLAGS = --no-print-directory

SCRIPT_LINES=@cc_script_lines@
HIJACKER_CONF_LINES=@cc_hijacker_conf_lines@
HAVE_MIXED_SS=@cc_have_mixed_ss@


# Install the compiler!


.PHONY: rootsim-cc

dist_bin_SCRIPTS = rootsim-cc
CLEANFILES = $(dist_bin_SCRIPTS) 
EXTRA_DIST = hijacker-conf.xml


do_subst = $(SED)	-e 's,[@]bindir[@],$(bindir),g' \
	   	  	-e 's,[@]libdir[@],$(libdir),g' \
			-e 's,[@]includedir[@],$(includedir),g' \
			-e 's,[@]datadir[@],$(datadir),g' \
			-e 's,[@]VERSION[@],$(VERSION),g' \
			-e 's,[@]CC[@],$(CC),g' \
			-e 's,[@]CFLAGS[@],$(CFLAGS),g' \
			-e 's,[@]ARCH[@],$(ARCH),g' \
			-e 's,[@]have_mixed_ss[@],$(HAVE_MIXED_SS),g' \
			-e 's,[@]scriptlines[@],$(SCRIPT_LINES),g' \
			-e 's,[@]hijacker_conf_lines[@],$(HIJACKER_CONF_LINES),g'

rootsim-cc: rootsim-cc.in
	@echo "Building compiler..."
	@$(do_subst) < rootsim-cc.in > rootsim-cc.pre
	@cat rootsim-cc.pre hijacker-conf.xml > rootsim-cc
	@rm rootsim-cc.pre
	@chmod +x rootsim-cc

