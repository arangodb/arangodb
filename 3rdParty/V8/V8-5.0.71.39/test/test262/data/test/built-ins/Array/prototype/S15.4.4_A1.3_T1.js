// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Array prototype object has a length property
es5id: 15.4.4_A1.3_T1
description: Array.prototype.length === 0
---*/

//CHECK#1
if (Array.prototype.length !== 0) {
  $ERROR('#1.1: Array.prototype.length === 0. Actual: ' + (Array.prototype.length));
}
