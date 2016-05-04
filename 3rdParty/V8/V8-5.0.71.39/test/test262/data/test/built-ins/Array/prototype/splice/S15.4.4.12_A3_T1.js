// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Check ToLength(length) for non Array objects
es5id: 15.4.4.12_A3_T1
description: length is arbitrarily
---*/

var obj = {};
obj.splice = Array.prototype.splice;
obj[0] = "x";
obj[4294967295] = "y";
obj.length = 4294967296;
var arr = obj.splice(4294967295, 1);

//CHECK#1
if (arr.length !== 1) {
  $ERROR('#1: var obj = {}; obj.splice = Array.prototype.splice; obj[0] = "x"; obj[4294967295] = "y"; obj.length = 4294967296; var arr = obj.splice(4294967295,1); arr.length === 1. Actual: ' + (arr.length));
}

//CHECK#2
if (obj.length !== 4294967295) {
   $ERROR('#2: var obj = {}; obj.splice = Array.prototype.splice; obj[0] = "x"; obj[4294967295] = "y"; obj.length = 4294967296; var arr = obj.splice(4294967295,1); obj.length === 4294967295. Actual: ' + (obj.length));
}

//CHECK#3
if (obj[0] !== "x") {
   $ERROR('#3: var obj = {}; obj.splice = Array.prototype.splice; obj[0] = "x"; obj[4294967295] = "y"; obj.length = 4294967296; var arr = obj.splice(4294967295,1); obj[0] === "x". Actual: ' + (obj[0]));
}   

//CHECK#4
if (obj[4294967295] !== undefined) {
   $ERROR('#4: var obj = {}; obj.splice = Array.prototype.splice; obj[0] = "x"; obj[4294967295] = "y"; obj.length = 4294967296; var arr = obj.splice(4294967295,1); obj[4294967295] === undefined. Actual: ' + (obj[4294967295]));
}  

//CHECK#5
if (arr[0] !== "y") {
   $ERROR('#5: var obj = {}; obj.splice = Array.prototype.splice; obj[0] = "x"; obj[4294967295] = "y"; obj.length = 4294967296; var arr = obj.splice(4294967295,1); arr[0] === "y". Actual: ' + (arr[0]));
}
