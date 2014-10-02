'use strict';


var assert = require('assert');
var yaml = require('../../');
var readFileSync = require('fs').readFileSync;


test("Incorrect utf-8 handling on require('file.yaml')", function () {
  var data = yaml.safeLoad(readFileSync(__dirname + '/0054.yml', 'utf8')),
      expected = '',
      index;

  //
  // document is an array of 40 elements
  // each element is a string of 100 `у` (Russian letter) chars
  //
  for (index = 0; index <= 100; index += 1) {
    expected += 'у';
  }

  //
  // make sure none of the strings were corrupted.
  //
  for (index = 0; index < 40; index += 1) {
    assert.equal(data[index], expected, ('Line ' + index + ' is corrupted'));
  }
});
