'use strict';


var assert = require('assert');
var yaml = require('../../');
var readFileSync = require('fs').readFileSync;


test('Timestamp parsing is one month off', function () {
  var data = yaml.safeLoad(readFileSync(__dirname + '/0019.yml', 'utf8'));

  // JS month starts with 0 (0 => Jan, 1 => Feb, ...)
  assert.equal(data.xmas.getTime(), Date.UTC(2011, 11, 24));
});
