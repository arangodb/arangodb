// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    If y<0 and y is finite and x is equal to -Infinity, Math.atan2(y,x) is an
    implementation-dependent approximation to -PI
es5id: 15.8.2.5_A17
description: >
    Checking if Math.atan2(y,x) is an approximation to -PI, where y<0
    and y is finite and x is equal to -Infinity
includes:
    - math_precision.js
    - math_isequal.js
---*/

// CHECK#1
var x = -Infinity;
var y = new Array();
y[0] = -0.000000000000001;
y[1] = -1;
y[2] = -1.7976931348623157E308; //largest (by module) finite number 
var ynum = 3;

for (var i = 0; i < ynum; i++)
{
	if (!isEqual(Math.atan2(y[i],x), -Math.PI))
		$ERROR("#1: Math.abs(Math.atan2(" + y[i] + ", " + x + ") + Math.PI) >= " + prec);
}
