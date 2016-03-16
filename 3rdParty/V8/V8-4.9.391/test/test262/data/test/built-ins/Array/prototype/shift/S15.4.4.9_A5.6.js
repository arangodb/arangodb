// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The shift property of Array has not prototype property
es5id: 15.4.4.9_A5.6
description: Checking Array.prototype.shift.prototype
---*/

//CHECK#1
if (Array.prototype.shift.prototype !== undefined) {
  $ERROR('#1: Array.prototype.shift.prototype === undefined. Actual: ' + (Array.prototype.shift.prototype));
}
