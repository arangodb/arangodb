// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The concat property of Array has not prototype property
es5id: 15.4.4.4_A4.6
description: Checking Array.prototype.concat.prototype
---*/

//CHECK#1
if (Array.prototype.concat.prototype !== undefined) {
  $ERROR('#1: Array.prototype.concat.prototype === undefined. Actual: ' + (Array.prototype.concat.prototype));
}
