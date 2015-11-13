ACLOCAL_AMFLAGS = -I m4 
AM_MAKEFLAGS = --no-print-directory

# Check whether we have to enable using hijacker in rootsim-cc
if USE_HIJACKER
CALL_HIJACKER=1
else
CALL_HIJACKER=0
endif

SCRIPT_LINES=@cc_script_lines@
HIJACKER_CONF_LINES=@cc_hijacker_conf_lines@


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
			-e 's,[@]VERSION[@],$(VERSION),g' \
			-e 's,[@]CC[@],$(CC),g' \
			-e 's,[@]CFLAGS[@],$(CFLAGS),g' \
			-e 's,[@]ARCH[@],$(ARCH),g' \
			-e 's,[@]scriptlines[@],$(SCRIPT_LINES),g' \
			-e 's,[@]hijackerconflines[@],$(HIJACKER_CONF_LINES),g' \
			-e 's,[@]use_hijacker[@],$(CALL_HIJACKER),g'

rootsim-cc: rootsim-cc.in
	@echo "Building compiler..."
	@$(do_subst) < rootsim-cc.in > rootsim-cc.pre
	@cat rootsim-cc.pre hijacker-conf.xml > rootsim-cc
	@rm rootsim-cc.pre
	@chmod +x rootsim-cc

