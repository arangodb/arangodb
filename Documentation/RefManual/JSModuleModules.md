JavaScript Modules {#JSModules}
===============================

@EMBEDTOC{JSModulesTOC}

Javascript Modules {#JSModulesIntro}
====================================

The ArangoDB uses a <a href="http://wiki.commonjs.org/wiki/Modules">CommonJS</a>
compatible module concept. You can use the function @FN{require} in order to
load a module. @FN{require} returns the exported variables and functions of the
module. You can use the option @CO{startup.modules-path} to specify the location
of the JavaScript files.

@anchor JSModulesRequire
@copydetails JSF_require
