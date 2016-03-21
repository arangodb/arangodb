// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Check ToLength(length) for non Array objects
es5id: 15.4.4.6_A3_T3
description: length = -1
---*/

var obj = {};
obj.pop = Array.prototype.pop;
obj[4294967294] = "x";
obj.length = -1;

//CHECK#1
var pop = obj.pop();
if (pop !== undefined) {
  $ERROR('#1: var obj = {}; obj.pop = Array.prototype.pop; obj[4294967294] = "x"; obj.length = -1; obj.pop() === undefined. Actual: ' + (pop));
}

//CHECK#2
if (obj.length !== 0) {
  $ERROR('#2: var obj = {}; obj.pop = Array.prototype.pop; obj[4294967294] = "x"; obj.length = -1; obj.pop(); obj.length === 0. Actual: ' + (obj.length));
}

//CHECK#3
if (obj[4294967294] !== "x") {
   $ERROR('#3: var obj = {}; obj.pop = Array.prototype.pop; obj[4294967294] = "x"; obj.length = -1; obj.pop(); obj[4294967294] === "x". Actual: ' + (obj[4294967294]));
}
