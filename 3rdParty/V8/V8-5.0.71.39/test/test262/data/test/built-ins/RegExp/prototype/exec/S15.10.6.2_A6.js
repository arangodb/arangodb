// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: RegExp.prototype.exec has not prototype property
es5id: 15.10.6.2_A6
description: Checking RegExp.prototype.exec.prototype
---*/

//CHECK#1
if (RegExp.prototype.exec.prototype !== undefined) {
  $ERROR('#1: RegExp.prototype.exec.prototype === undefined. Actual: ' + (RegExp.prototype.exec.prototype));
}
