
var assert = require('assert');
var iso = require('..');

describe('to-iso-string', function () {
  it('should work', function () {
    var date = new Date("05 October 2011 14:48 UTC");
    assert('2011-10-05T14:48:00.000Z' == iso(date));
  });
});