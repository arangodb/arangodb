'use strict';


var assert = require('assert');
var yaml   = require('../../');


test('Invalid errors/warnings of invalid indentation on flow scalars', function () {
  var sources = [
    'text:\n    hello\n  world',   // plain style
    "text:\n    'hello\n  world'", // single-quoted style
    'text:\n    "hello\n  world"'  // double-quoted style
  ];
  var expected = { text: 'hello world' };

  assert.doesNotThrow(function () { yaml.load(sources[0]); }, 'Throws on plain style');
  assert.doesNotThrow(function () { yaml.load(sources[1]); }, 'Throws on single-quoted style');
  assert.doesNotThrow(function () { yaml.load(sources[2]); }, 'Throws on double-quoted style');

  assert.deepEqual(yaml.load(sources[0]), expected);
  assert.deepEqual(yaml.load(sources[1]), expected);
  assert.deepEqual(yaml.load(sources[2]), expected);
});
