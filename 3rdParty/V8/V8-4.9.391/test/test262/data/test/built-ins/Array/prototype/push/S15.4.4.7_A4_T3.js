// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Check ToLength(length) for non Array objects
es5id: 15.4.4.7_A4_T3
description: length = -1
---*/

var obj = {};
obj.push = Array.prototype.push;
obj.length = -1;

//CHECK#1
var push = obj.push("x", "y", "z");
if (push !== 3) {
  $ERROR('#1: var obj = {}; obj.push = Array.prototype.push; obj.length = -1; obj.push("x", "y", "z") === 3. Actual: ' + (push));
}

//CHECK#2
if (obj.length !== 3) {
  $ERROR('#2: var obj = {}; obj.push = Array.prototype.push; obj.length = -1; obj.push("x", "y", "z"); obj.length === 3. Actual: ' + (obj.length));
}

//CHECK#3
if (obj[4294967295] !== undefined) {
   $ERROR('#3: var obj = {}; obj.push = Array.prototype.push; obj.length = -1; obj.push("x", "y", "z"); obj[4294967295] === undefined. Actual: ' + (obj[4294967295]));
}

//CHECK#4
if (obj[4294967296] !== undefined) {
   $ERROR('#4: var obj = {}; obj.push = Array.prototype.push; obj.length = -1; obj.push("x", "y", "z"); obj[4294967296] === undefined. Actual: ' + (obj[4294967296]));
}  

//CHECK#5
if (obj[4294967297] !== undefined) {
   $ERROR('#5: var obj = {}; obj.push = Array.prototype.push; obj.length = -1; obj.push("x", "y", "z"); obj[4294967297] === undefined. Actual: ' + (obj[4294967297]));
}  

//CHECK#6
if (obj[0] !== "x") {
   $ERROR('#3: var obj = {}; obj.push = Array.prototype.push; obj.length = -1; obj.push("x", "y", "z"); obj[0] === "x". Actual: ' + (obj[0]));
}

//CHECK#7
if (obj[1] !== "y") {
   $ERROR('#4: var obj = {}; obj.push = Array.prototype.push; obj.length = -1; obj.push("x", "y", "z"); obj[1] === "y". Actual: ' + (obj[1]));
}

//CHECK#8
if (obj[2] !== "z") {
   $ERROR('#5: var obj = {}; obj.push = Array.prototype.push; obj.length = -1; obj.push("x", "y", "z"); obj[2] === "z". Actual: ' + (obj[2]));
}
