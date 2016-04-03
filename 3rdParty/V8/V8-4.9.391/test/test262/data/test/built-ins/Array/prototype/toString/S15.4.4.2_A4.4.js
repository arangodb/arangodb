// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of toString is 0
es5id: 15.4.4.2_A4.4
description: toString.length === 1
---*/

//CHECK#1
if (Array.prototype.toString.length !== 0) {
  $ERROR('#1: Array.prototype.toString.length === 0. Actual: ' + (Array.prototype.toString.length));
}
