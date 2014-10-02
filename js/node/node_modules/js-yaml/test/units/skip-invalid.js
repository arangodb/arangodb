'use strict';


var assert = require('assert');
var yaml   = require('../../');


var sample = {
  number: 42,
  undef:  undefined,
  string: 'hello',
  func:   function (a, b) { return a + b; },
  regexp: /^hel+o/,
  array:  [1, 2, 3]
};


var expected = {
  number: 42,
  string: 'hello',
  array:  [1, 2, 3]
};


test('Dumper must throw an exception on invalid type when option `skipInvalid` is false.', function () {
  assert.throws(function () {
    yaml.safeDump(sample, { skipInvalid: false });
  }, yaml.YAMLException);
});


test('Dumper must skip pairs and values with invalid types when option `skipInvalid` is true.', function () {
  assert.deepEqual(yaml.load(yaml.safeDump(sample, { skipInvalid: true })), expected);
});
