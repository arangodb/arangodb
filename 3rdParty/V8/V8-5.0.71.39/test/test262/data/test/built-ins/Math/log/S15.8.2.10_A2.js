// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If x is less than 0, Math.log(x) is NaN
es5id: 15.8.2.10_A2
description: Checking if Math.log(x) is NaN, where x is less than 0
---*/

// CHECK#1
var x = -0.000000000000001;
if (!isNaN(Math.log(x)))
{
	$ERROR("#1: 'var x=-0.000000000000001; isNaN(Math.log(x)) === false'");
}

// CHECK#2
x = -1;
if (!isNaN(Math.log(x)))
{
	$ERROR("#1: 'var x=-1; isNaN(Math.log(x)) === false'");
}

// CHECK#3
x = -Infinity;
if (!isNaN(Math.log(x)))
{
	$ERROR("#1: 'var x=-Infinity; isNaN(Math.log(x)) === false'");
}
