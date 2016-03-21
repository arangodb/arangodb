// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The unshift property of Array has not prototype property
es5id: 15.4.4.13_A5.6
description: Checking Array.prototype.unshift.prototype
---*/

//CHECK#1
if (Array.prototype.unshift.prototype !== undefined) {
  $ERROR('#1: Array.prototype.unshift.prototype === undefined. Actual: ' + (Array.prototype.unshift.prototype));
}
