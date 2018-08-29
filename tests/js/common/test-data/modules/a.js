'use strict';

let internal = require("internal");

var j;
for (j = 0;  j < 5;  ++j) {
  internal.sleep(1);
}

exports.c = 10;
exports.a = require("b").b;
