'use strict';


var assert = require('assert');
var yaml = require('../../');
var readFileSync = require('fs').readFileSync;


test('refactor compact variant of MarkedYAMLError.toString', function () {
  var source = readFileSync(__dirname + '/0033.yml', 'utf8');

  assert.throws(function () {
    yaml.safeLoad(source);
  }, "require('issue-33.yml') should throw, but it does not");
});
