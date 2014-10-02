'use strict';


var assert = require('assert');


function testHandler(actual) {
  var expected = testHandler.expected;

  assert.strictEqual(actual.length, expected.length);

  assert.strictEqual(
    actual[0](),
    expected[0]());

  assert.strictEqual(
    actual[1](10, 20),
    expected[1](10, 20));

  assert.deepEqual(
    actual[2]('book'),
    expected[2]('book'));
}

testHandler.expected = [
  function () {
    return 42;
  },
  function (x, y) {
    return x + y;
  },
  function (foo) {
    var result = 'There is my ' + foo + ' at the table.';

    return {
      first: 42,
      second: 'sum',
      third: result
    };
  }
];


module.exports = testHandler;
