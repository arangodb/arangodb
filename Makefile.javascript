# -*- mode: Makefile; -*-

################################################################################
## JavaScript source code as header
################################################################################

BUILT_SOURCES += .setup-directories

.setup-directories:
	@test -d js || mkdir js
	@test -d js/bootstrap || mkdir js/bootstrap
	@test -d js/modules || mkdir js/modules
	@test -d js/server || mkdir js/server
	@touch $@

js/js-%.h: @srcdir@/js/%.js .setup-directories
	@top_srcdir@/config/js2c.sh $< > $@

js/bootstrap/js-%.h: @srcdir@/js/bootstrap/%.js .setup-directories
	@top_srcdir@/config/js2c.sh $< > $@

js/server/js-%.h: @srcdir@/js/server/%.js .setup-directories
	@top_srcdir@/config/js2c.sh $< > $@

################################################################################
## CLEANUP
################################################################################

CLEANUP += $(JAVASCRIPT_HEADER)
