################################################################################
## GNUmakefile for AvocadoDB
################################################################################

-include Makefile

################################################################################
# setup
################################################################################

setup:
	@echo ACLOCAL
	@aclocal -I m4
	@echo AUTOMAKE
	@automake --add-missing --force-missing --copy
	@echo AUTOCONF
	@autoconf -I m4
	@echo auto system configured, proceed with configure

touch:
	test -f configure
	touch configure
	test -f aclocal.m4
	touch aclocal.m4
	test -f Makefile.in
	touch Makefile.in
	touch config/*
