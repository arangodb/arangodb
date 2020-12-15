/* eslint-disable */
;(function () {
  'use strict'
  /* eslint-enable */

  global.SCAFFOLDING_MODULES = {};

  global.DEFINE_MODULE = function (path, module) {
    global.SCAFFOLDING_MODULES[path] = module;
  };

  global.require = function (path) {
    return global.SCAFFOLDING_MODULES[path];
  };
}());
