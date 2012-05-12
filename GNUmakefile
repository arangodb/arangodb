################################################################################
## GNUmakefile for ArangoDB
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
