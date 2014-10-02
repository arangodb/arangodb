'use strict';


var assert = require('assert');
var yaml = require('../../');
var readFileSync = require('fs').readFileSync;


test('Parse failed when no document start present', function () {
  assert.doesNotThrow(function () {
    yaml.safeLoad(readFileSync(__dirname + '/0008.yml', 'utf8'));
  }, TypeError);
});
