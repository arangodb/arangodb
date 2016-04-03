// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    If y<0 and x is +0, Math.atan2(y,x) is an implementation-dependent
    approximation to -PI/2
es5id: 15.8.2.5_A12
description: >
    Checking if Math.atan2(y,+0) is an approximation to -PI/2, where
    y<0
includes:
    - math_precision.js
    - math_isequal.js
---*/

// CHECK#1
var x = +0;
//prec = 0.00000000000001;
var y = new Array();
y[0] = -0.000000000000001;
y[2] = -Infinity;
y[1] = -1; 
var ynum = 3;

for (var i = 0; i < ynum; i++)
{
	if (!isEqual(Math.atan2(y[i],x), -(Math.PI)/2))
		$ERROR("#1: Math.abs(Math.atan2(" + y[i] + ", " + x + ") + ((Math.PI)/2)) >= " + prec);
}
