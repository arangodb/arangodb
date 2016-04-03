// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Check ToLength(length) for non Array objects
es5id: 15.4.4.5_A4_T3
description: length = -4294967294
---*/

var obj = {};
obj.join = Array.prototype.join;
obj[0] = "x";
obj[1] = "y";
obj[2] = "z";
obj.length = -4294967294;

//CHECK#1
if (obj.join("") !== "") {
  $ERROR('#1: var obj = {}; obj.join = Array.prototype.join; obj[0] = "x"; obj[1] = "y"; obj[2] = "z"; obj.length = -4294967294; obj.join("") === "". Actual: ' + (obj.join("")));
}

//CHECK#2
if (obj.length !== -4294967294) {
  $ERROR('#2: var obj = {}; obj.join = Array.prototype.join; obj[0] = "x"; obj[1] = "y"; obj[2] = "z"; obj.length = -4294967294; obj.join(""); obj.length === -4294967294. Actual: ' + (obj.length));
}
