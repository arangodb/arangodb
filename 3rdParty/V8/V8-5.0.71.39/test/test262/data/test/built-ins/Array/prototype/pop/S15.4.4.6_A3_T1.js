// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Check ToLength(length) for non Array objects
es5id: 15.4.4.6_A3_T1
description: length = 4294967296
---*/

var obj = {};
obj.pop = Array.prototype.pop;
obj[0] = "x";
obj[4294967295] = "y";
obj.length = 4294967296;

//CHECK#1
var pop = obj.pop();
if (pop !== "y") {
  $ERROR('#1: var obj = {}; obj.pop = Array.prototype.pop; obj[0] = "x"; obj[4294967295] = "y"; obj.length = 4294967296; obj.pop() === "y". Actual: ' + (pop));
}

//CHECK#2
if (obj.length !== 4294967295) {
  $ERROR('#2: var obj = {}; obj.pop = Array.prototype.pop; obj[0] = "x"; obj[4294967295] = "y"; obj.length = 4294967296; obj.pop(); obj.length === 4294967295. Actual: ' + (obj.length));
}

//CHECK#3
if (obj[0] !== "x") {
   $ERROR('#3: var obj = {}; obj.pop = Array.prototype.pop; obj[0] = "x"; obj[4294967295] = "y"; obj.length = 4294967296; obj.pop(); obj[0] === "x". Actual: ' + (obj[0]));
}  

//CHECK#4
if (obj[4294967295] !== undefined) {
   $ERROR('#4: var obj = {}; obj.pop = Array.prototype.pop; obj[0] = "x"; obj[4294967295] = "y"; obj.length = 4294967296; obj.pop(); obj[4294967295] === undefined. Actual: ' + (obj[4294967295]));
}
