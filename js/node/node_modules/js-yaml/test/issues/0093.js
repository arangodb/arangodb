'use strict';


var assert = require('assert');
var yaml = require('../../');
var readFileSync = require('fs').readFileSync;


test('Unwanted line breaks in folded scalars', function () {
  var data = yaml.safeLoad(readFileSync(__dirname + '/0093.yml', 'utf8'));

  assert.strictEqual(data.first,  'a b\n  c\n  d\ne f\n');
  assert.strictEqual(data.second, 'a b\n  c\n\n  d\ne f\n');
  assert.strictEqual(data.third,  'a b\n\n  c\n  d\ne f\n');
});
