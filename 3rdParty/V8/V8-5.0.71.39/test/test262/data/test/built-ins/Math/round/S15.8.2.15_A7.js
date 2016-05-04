// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    If x is less than or equal to -0 and x is greater than or equal to -0.5,
    Math.round(x) is equal to -0
es5id: 15.8.2.15_A7
description: >
    `Math.round(x)` differs from `Math.floor(x + 0.5)`:

    1) for values in [-0.5; -0]
    2) for 0.5 - Number.EPSILON / 4
    3) for odd integers in [-(2 / Number.EPSILON - 1); -(1 / Number.EPSILON + 1)] or in [1 / Number.EPSILON + 1; 2 / Number.EPSILON - 1]
---*/

// CHECK#1
if (1 / Math.round(-0.5) !== 1 / -0) {
  $ERROR("#1: '1 / Math.round(-0.5) !== 1 / -0'");
}

// CHECK#2
if (1 / Math.round(-0.25) !== 1 / -0) {
	$ERROR("#2: '1 / Math.round(-0.25) !== 1 / -0'");
}

// CHECK#3
if (1 / Math.round(-0) !== 1 / -0) {
  $ERROR("#3: '1 / Math.round(-0) !== 1 / -0'");
}

var x = 0;

// CHECK#4
x = 0.5 - Number.EPSILON / 4;
if (1 / Math.round(x) !== 1 / 0) {
  $ERROR("#4: '1 / Math.round(" + x + ") !== 1 / 0'");
}

// CHECK#5
x = -(2 / Number.EPSILON - 1);
if (Math.round(x) !== x) {
  $ERROR("#5: 'Math.round(" + x + ") !== " + x + "'");
}

// CHECK#6
x = -(1.5 / Number.EPSILON - 1);
if (Math.round(x) !== x) {
  $ERROR("#6: 'Math.round(" + x + ") !== " + x + "'");
}

// CHECK#7
x = -(1 / Number.EPSILON + 1);
if (Math.round(x) !== x) {
  $ERROR("#7: 'Math.round(" + x + ") !== " + x + "'");
}

// CHECK#8
x = 1 / Number.EPSILON + 1;
if (Math.round(x) !== x) {
  $ERROR("#8: 'Math.round(" + x + ") !== " + x + "'");
}

// CHECK#9
x = 1.5 / Number.EPSILON - 1;
if (Math.round(x) !== x) {
  $ERROR("#9: 'Math.round(" + x + ") !== " + x + "'");
}

// CHECK#10
x = 2 / Number.EPSILON - 1;
if (Math.round(x) !== x) {
  $ERROR("#10: 'Math.round(" + x + ") !== " + x + "'");
}
