'use strict';


var fs   = require('fs');
var util = require('util');
var yaml = require('./lib/js-yaml.js');


require.extensions['.yml'] = require.extensions['.yaml'] =
  util.deprecate(function (m, f) {
    m.exports = yaml.safeLoad(fs.readFileSync(f, 'utf8'), { filename: f });
  }, 'Direct yaml files load via require() is deprecated! Use safeLoad() instead.');


module.exports = yaml;
