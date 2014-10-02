'use strict';


var assert = require('assert');
var yaml   = require('../../');


test('RegExps should be properly closed', function () {
  assert.throws(function () { yaml.load('!!js/regexp /fo'); });
  assert.throws(function () { yaml.load('!!js/regexp /fo/q'); });
  assert.throws(function () { yaml.load('!!js/regexp /fo/giii'); });

  assert.equal(yaml.load('!!js/regexp /fo/g/g'), '/fo/g/g');
});
