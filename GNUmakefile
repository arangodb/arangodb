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
	@echo adding changes to GIT
	git add Makefile.in aclocal.m4 config/compile config/config.guess config/config.sub config/depcomp config/install-sh config/missing
	@echo auto system configured, proceed with configure
