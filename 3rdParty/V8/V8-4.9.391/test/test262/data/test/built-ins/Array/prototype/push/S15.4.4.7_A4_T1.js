// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Check ToLength(length) for non Array objects
es5id: 15.4.4.7_A4_T1
description: length = 4294967296
---*/

var obj = {};
obj.push = Array.prototype.push;
obj.length = 4294967296;

//CHECK#1
var push = obj.push("x", "y", "z");
if (push !== 4294967299) {
  $ERROR('#1: var obj = {}; obj.push = Array.prototype.push; obj.length = 4294967296; obj.push("x", "y", "z") === 4294967299. Actual: ' + (push));
}

//CHECK#2
if (obj.length !== 4294967299) {
  $ERROR('#2: var obj = {}; obj.push = Array.prototype.push; obj.length = 4294967296; obj.push("x", "y", "z"); obj.length === 4294967299. Actual: ' + (obj.length));
}

//CHECK#3
if (obj[0] !== undefined) {
   $ERROR('#3: var obj = {}; obj.push = Array.prototype.push; obj.length = 4294967296; obj.push("x", "y", "z"); obj[0] === undefined. Actual: ' + (obj[0]));
}

//CHECK#4
if (obj[1] !== undefined) {
   $ERROR('#4: var obj = {}; obj.push = Array.prototype.push; obj.length = 4294967296; obj.push("x", "y", "z"); obj[1] === undefined. Actual: ' + (obj[1]));
}  

//CHECK#5
if (obj[2] !== undefined) {
   $ERROR('#5: var obj = {}; obj.push = Array.prototype.push; obj.length = 4294967296; obj.push("x", "y", "z"); obj[2] === undefined. Actual: ' + (obj[2]));
} 

//CHECK#6
if (obj[4294967296] !== "x") {
   $ERROR('#6: var obj = {}; obj.push = Array.prototype.push; obj.length = 4294967296; obj.push("x", "y", "z"); obj[4294967296] === "x". Actual: ' + (obj[4294967296]));
}

//CHECK#7
if (obj[4294967297] !== "y") {
   $ERROR('#7: var obj = {}; obj.push = Array.prototype.push; obj.length = 4294967296; obj.push("x", "y", "z"); obj[4294967297] === "y". Actual: ' + (obj[4294967297]));
}  

//CHECK#8
if (obj[4294967298] !== "z") {
   $ERROR('#8: var obj = {}; obj.push = Array.prototype.push; obj.length = 4294967296; obj.push("x", "y", "z"); obj[4294967298] === "z". Actual: ' + (obj[4294967298]));
} 

var obj = {};
obj.push = Array.prototype.push;
obj.length = 4294967296;

//CHECK#9
var push = obj.push();
if (push !== 4294967296) {
  $ERROR('#9: var obj = {}; obj.push = Array.prototype.push; obj.length = 4294967296; obj.push() === 4294967296. Actual: ' + (push));
}

//CHECK#10
if (obj.length !== 4294967296) {
  $ERROR('#10: var obj = {}; obj.push = Array.prototype.push; obj.length = 4294967296; obj.push(); obj.length === 4294967296. Actual: ' + (obj.length));
}
