// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Check ToLength(length) for non Array objects
es5id: 15.4.4.9_A3_T3
description: length is arbitrarily
---*/

var obj = {};
obj.shift = Array.prototype.shift;
obj[0] = "x";
obj[1] = "y";
obj.length = -4294967294;

//CHECK#1
var shift = obj.shift();
if (shift !== undefined) {
  $ERROR('#1: var obj = {}; obj.shift = Array.prototype.shift; obj[0] = "x"; obj[1] = "y"; obj.length = -4294967294; obj.shift() === undefined. Actual: ' + (shift));
}

//CHECK#2
if (obj.length !== 0) {
  $ERROR('#2: var obj = {}; obj.shift = Array.prototype.shift; obj[0] = "x"; obj[1] = "y"; obj.length = -4294967294; obj.shift(); obj.length === 0. Actual: ' + (obj.length));
}

//CHECK#3
if (obj[0] !== "x") {
   $ERROR('#3: var obj = {}; obj.shift = Array.prototype.shift; obj[0] = "x"; obj[1] = "y"; obj.length = -4294967294; obj.shift(); obj[0] === "x". Actual: ' + (obj[0]));
}

//CHECK#4
if (obj[1] !== "y") {
   $ERROR('#4: var obj = {}; obj.shift = Array.prototype.shift; obj[0] = "x" obj[1] = "y"; obj.length = -4294967294; obj.shift(); obj[1] === "y". Actual: ' + (obj[1]));
}
