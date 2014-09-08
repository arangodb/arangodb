'use strict';


var assert = require('assert');
var yaml   = require('../../');


test('Plain scalar "constructor" parsed as `null`', function () {
  assert.strictEqual(yaml.load('constructor'),          'constructor');
  assert.deepEqual(yaml.load('constructor: value'),     { 'constructor': 'value' });
  assert.deepEqual(yaml.load('key: constructor'),       { 'key': 'constructor' });
  assert.deepEqual(yaml.load('{ constructor: value }'), { 'constructor': 'value' });
  assert.deepEqual(yaml.load('{ key: constructor }'),   { 'key': 'constructor' });
});
