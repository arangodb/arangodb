// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Check ToLength(length) for non Array objects
es5id: 15.4.4.12_A3_T3
description: length is arbitrarily
---*/

var obj = {};
obj.splice = Array.prototype.splice;
obj[4294967294] = "x";
obj.length = -1;
var arr = obj.splice(4294967294,1);

//CHECK#1
if (arr.length !== 0) {
  $ERROR('#1: var obj = {}; obj.splice = Array.prototype.splice; obj[4294967294] = "x"; obj.length = -1; var arr = obj.splice(4294967294,1); arr.length === 0. Actual: ' + (arr.length));
}

//CHECK#2
if (arr[0] !== undefined) {
   $ERROR('#2: var obj = {}; obj.splice = Array.prototype.splice; obj[4294967294] = "x"; obj.length = 1; var arr = obj.splice(4294967294,1); arr[0] === undefined. Actual: ' + (arr[0]));
} 

//CHECK#3
if (obj.length !== 0) {
   $ERROR('#3: var obj = {}; obj.splice = Array.prototype.splice; obj[4294967294] = "x"; obj.length = 1; var arr = obj.splice(4294967294,1); obj.length === 0. Actual: ' + (obj.length));
}

//CHECK#4
if (obj[4294967294] !== "x") {
   $ERROR('#4: var obj = {}; obj.splice = Array.prototype.splice; obj[4294967294] = "x"; obj.length = 1; var arr = obj.splice(4294967294,1); obj[4294967294] === "x". Actual: ' + (obj[4294967294]));
}
