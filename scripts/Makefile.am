ACLOCAL_AMFLAGS = -I m4 
AM_MAKEFLAGS = --no-print-directory

SCRIPT_LINES=@cc_script_lines@
LD_DATA1_LINES=@cc_lddata1_lines@
LD_DATA2_LINES=@cc_lddata2_lines@
LD_FINAL_LINES=@cc_ldfinal_lines@


# Install the compiler!


.PHONY: rootsim-cc

dist_bin_SCRIPTS = rootsim-cc
CLEANFILES = $(dist_bin_SCRIPTS) 
EXTRA_DIST = ld-data1 ld-data2 ld-final


do_subst = $(SED)	-e 's,[@]bindir[@],$(bindir),g' \
	   	  	-e 's,[@]libdir[@],$(libdir),g' \
			-e 's,[@]includedir[@],$(includedir),g' \
			-e 's,[@]datadir[@],$(datadir),g' \
			-e 's,[@]VERSION[@],$(VERSION),g' \
			-e 's,[@]CC[@],$(CC),g' \
			-e 's,[@]CFLAGS[@],$(CFLAGS),g' \
			-e 's,[@]ARCH[@],$(ARCH),g' \
			-e 's,[@]scriptlines[@],$(SCRIPT_LINES),g' \
			-e 's,[@]lddata1lines[@],$(LD_DATA1_LINES),g' \
			-e 's,[@]lddata2lines[@],$(LD_DATA2_LINES),g' \
			-e 's,[@]ldfinallines[@],$(LD_FINAL_LINES),g'

rootsim-cc: rootsim-cc.in
	@echo "Building compiler..."
	@$(do_subst) < rootsim-cc.in > rootsim-cc.pre
	@cat rootsim-cc.pre ld-data1 ld-data2 ld-final > rootsim-cc
	@rm rootsim-cc.pre
	@chmod +x rootsim-cc

