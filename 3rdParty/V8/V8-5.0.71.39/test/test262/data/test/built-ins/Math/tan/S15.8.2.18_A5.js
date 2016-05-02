// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If x is -Infinity, Math.tan(x) is NaN
es5id: 15.8.2.18_A5
description: Checking if Math.tan(-Infinity) is NaN
---*/

// CHECK#1
var x = -Infinity;
if (!isNaN(Math.tan(x)))
{
	$ERROR("#1: 'var x=-Infinity; isNaN(Math.tan(x)) === false'");
}
