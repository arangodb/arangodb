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

js/common/bootstrap/js-%.h: @srcdir@/js/common/bootstrap/%.js .setup-directories
	@top_srcdir@/config/js2c.sh $< > $@

js/client/js-%.h: @srcdir@/js/client/%.js .setup-directories
	@top_srcdir@/config/js2c.sh $< > $@

js/server/js-%.h: @srcdir@/js/server/%.js .setup-directories
	@top_srcdir@/config/js2c.sh $< > $@

html/admin/js/modules/%.js: @srcdir@/js/common/modules/%.js
	(echo "module.define(\"`basename $< .js`\", function(exports, module) {" && cat $< && echo "});") > $@

html/admin/js/modules/%.js: @srcdir@/js/client/modules/%.js
	(echo "module.define(\"`basename $< .js`\", function(exports, module) {" && cat $< && echo "});") > $@

################################################################################
## CLEANUP
################################################################################

CLEANUP += $(JAVASCRIPT_HEADER)
