// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Since Error prototype object is not function it has no [[Call]] method
es5id: 15.11.4_A3
description: Checking if call of Error prototype as a function fails
includes:
    - Test262Error.js
---*/

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
try {
	Error.prototype();
	$ERROR('#1: "Error.prototype()" lead to throwing exception');
} catch (e) {
    if (e instanceof Test262Error) throw e;
}
//
//////////////////////////////////////////////////////////////////////////////
