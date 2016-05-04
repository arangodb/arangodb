// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The sort property of Array has not prototype property
es5id: 15.4.4.11_A7.6
description: Checking Array.prototype.sort.prototype
---*/

//CHECK#1
if (Array.prototype.sort.prototype !== undefined) {
  $ERROR('#1: Array.prototype.sort.prototype === undefined. Actual: ' + (Array.prototype.sort.prototype));
}
