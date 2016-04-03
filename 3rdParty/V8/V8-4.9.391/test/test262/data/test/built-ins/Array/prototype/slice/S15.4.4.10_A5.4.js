// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of slice is 2
es5id: 15.4.4.10_A5.4
description: slice.length === 2
---*/

//CHECK#1
if (Array.prototype.slice.length !== 2) {
  $ERROR('#1: Array.prototype.slice.length === 2. Actual: ' + (Array.prototype.slice.length));
}
