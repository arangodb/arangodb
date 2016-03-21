// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of the call method is 1
es5id: 15.3.4.4_A2_T1
description: Checking Function.prototype.call.length
---*/

//CHECK#1
if (typeof Function.prototype.call !== "function") {
  $ERROR('#1: call method defined');
}

//CHECK#2
if (typeof Function.prototype.call.length === "undefined") {
  $ERROR('#2: length property of call method defined');
}

//CHECK#3
if (Function.prototype.call.length !== 1) {
  $ERROR('#3: The length property of the call method is 1');
}
