// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of the toString method is 1
es5id: 15.10.6.4_A11
description: Checking RegExp.prototype.toString.length
---*/

//CHECK#1
if (RegExp.prototype.toString.hasOwnProperty("length") !== true) {
	$ERROR('#1: RegExp.prototype.toString.hasOwnProperty(\'length\') === true');
}

//CHECK#2
if (RegExp.prototype.toString.length !== 0) {
	$ERROR('#2: RegExp.prototype.toString.length === 0. Actual: ' + (RegExp.prototype.toString.length));
}
