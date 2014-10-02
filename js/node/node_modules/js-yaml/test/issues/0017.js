'use strict';


var assert = require('assert');
var yaml = require('../../');
var readFileSync = require('fs').readFileSync;


test('Non-specific "!" tags should resolve to !!str', function () {
  var data = yaml.safeLoad(readFileSync(__dirname + '/0017.yml', 'utf8'));

  assert.equal(typeof data, 'string');
});
