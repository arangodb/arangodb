// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of the toString method is 0
es5id: 15.3.4.2_A11
description: Checking Function.prototype.toString.length
---*/

//CHECK#1
if (!(Function.prototype.toString.hasOwnProperty("length"))) {
  $ERROR('#1: The Function.prototype.toString has the length property');
}

//CHECK#2
if (Function.prototype.toString.length !== 0) {
  $ERROR('#2: The length property of the toString method is 0');
}
