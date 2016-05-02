// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: RegExp.prototype.toString has not prototype property
es5id: 15.10.6.4_A6
description: Checking RegExp.prototype.toString.prototype
---*/

//CHECK#1
if (RegExp.prototype.toString.prototype !== undefined) {
	$ERROR('#1: RegExp.prototype.toString.prototype === undefined. Actual: ' + (RegExp.prototype.toString.prototype));
}
