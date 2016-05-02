// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If y is NaN, Math.pow(x,y) is NaN
es5id: 15.8.2.13_A1
description: Checking if Math.pow(x,y) is NaN, where y is NaN
---*/

// CHECK#1

var y = NaN;
var x = new Array();
x[0] = -Infinity;
x[1] = -1.7976931348623157E308; //largest (by module) finite number
x[2] = -0.000000000000001;
x[3] = -0;
x[4] = +0
x[5] = 0.000000000000001;
x[6] = 1.7976931348623157E308; //largest finite number
x[7] = +Infinity;
x[8] = NaN;
var xnum = 9;

for (var i = 0; i < xnum; i++)
{
	if (!isNaN(Math.pow(x[i],y)))
	{
		$ERROR("#1: isNaN(Math.pow(" + x[i] + ", " + y + ")) === false");
	}
}
