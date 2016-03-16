// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    If y is equal to -0 and x<0, Math.atan2(y,x) is an
    implementation-dependent approximation to -PI
es5id: 15.8.2.5_A11
description: Checking if Math.atan2(-0,x) is an approximation to -PI, where x<0
includes:
    - math_precision.js
    - math_isequal.js
---*/

// CHECK#1
var y = -0;
//prec = 0.00000000000001;
var x = new Array();
x[0] = -0.000000000000001;
x[2] = -Infinity;
x[1] = -1; 
var xnum = 3;

for (var i = 0; i < xnum; i++)
{
	if (!isEqual(Math.atan2(y,x[i]), - Math.PI))
		$ERROR("#1: Math.abs(Math.atan2(" + y + ", " + x[i] + ") + Math.PI) >= " + prec);
}
