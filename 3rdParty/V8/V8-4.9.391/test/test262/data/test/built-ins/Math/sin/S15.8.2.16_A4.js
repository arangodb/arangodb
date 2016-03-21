// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If x is +Infinity, Math.sin(x) is NaN
es5id: 15.8.2.16_A4
description: Checking if Math.sin(+Infinity) is NaN
---*/

// CHECK#1
var x = +Infinity;
if (!isNaN(Math.sin(x)))
{
	$ERROR("#1: 'var x = +Infinity; isNaN(Math.sin(x)) === false'");
}
