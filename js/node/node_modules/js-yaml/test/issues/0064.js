'use strict';


var assert = require('assert');
var yaml = require('../../');
var readFileSync = require('fs').readFileSync;


test('Wrong error message when yaml file contains tabs', function () {
  assert.doesNotThrow(
    function () { yaml.safeLoad(readFileSync(__dirname + '/0064.yml', 'utf8')); },
    yaml.YAMLException);
});
