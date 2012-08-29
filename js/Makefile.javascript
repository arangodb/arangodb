# -*- mode: Makefile; -*-

################################################################################
## --SECTION--                                                        JAVASCRIPT
################################################################################

################################################################################
### @brief sets up the directories
################################################################################

BUILT_SOURCES += .setup-js-directories

.setup-js-directories:
	@test -d html/admin/js/modules || mkdir html/admin/js/modules
	@test -d js || mkdir js
	@test -d js/client || mkdir js/client
	@test -d js/common/bootstrap || mkdir js/common/bootstrap
	@test -d js/server || mkdir js/server
	@touch $@

################################################################################
### @brief converts JavaScript files to header files
################################################################################

html/admin/js/modules/%.js: @srcdir@/js/common/modules/%.js .setup-js-directories
	(echo "module.define(\"`basename $< .js`\", function(exports, module) {" && cat $< && echo "});") > $@

html/admin/js/modules/%.js: @srcdir@/js/client/modules/%.js .setup-js-directories
	(echo "module.define(\"`basename $< .js`\", function(exports, module) {" && cat $< && echo "});") > $@

js/js-%.h: @srcdir@/js/%.js .setup-js-directories
	@top_srcdir@/config/js2c.sh $< > $@

js/client/js-%.h: @srcdir@/js/client/%.js .setup-js-directories
	@top_srcdir@/config/js2c.sh $< > $@

js/common/bootstrap/js-%.h: @srcdir@/js/common/bootstrap/%.js .setup-js-directories
	@top_srcdir@/config/js2c.sh $< > $@

js/server/js-%.h: @srcdir@/js/server/%.js .setup-js-directories
	@top_srcdir@/config/js2c.sh $< > $@

################################################################################
### @brief cleanup
################################################################################

CLEANUP += $(JAVASCRIPT_HEADER) .setup-js-directories

################################################################################
## --SECTION--                                                       END-OF-FILE
################################################################################

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
