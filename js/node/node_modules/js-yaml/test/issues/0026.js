'use strict';


var assert = require('assert');
var yaml = require('../../');
var readFileSync = require('fs').readFileSync;


test('should convert new line into white space', function () {
  var data = yaml.safeLoad(readFileSync(__dirname + '/0026.yml', 'utf8'));

  assert.equal(data.test, 'a b c\n');
});
