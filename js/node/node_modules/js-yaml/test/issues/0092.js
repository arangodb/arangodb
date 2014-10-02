'use strict';


var assert = require('assert');
var yaml   = require('../../');


test('Invalid parse error on whitespace between quoted scalar keys and ":" symbol in mappings', function () {
  assert.doesNotThrow(function () {
    yaml.load('{ "field1" : "v1", "field2": "v2" }');
  });
});
