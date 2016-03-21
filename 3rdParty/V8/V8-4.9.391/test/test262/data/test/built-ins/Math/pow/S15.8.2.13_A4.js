// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If x is NaN and y is nonzero, Math.pow(x,y) is NaN
es5id: 15.8.2.13_A4
description: Checking if Math.pow(x,y) is NaN, where x is NaN and y is nonzero
---*/

// CHECK#1

var x = NaN;
var y = new Array();
y[0] = -Infinity;
y[1] = -1.7976931348623157E308; //largest (by module) finite number
y[2] = -0.000000000000001;
y[3] = 0.000000000000001;
y[4] = 1.7976931348623157E308; //largest finite number
y[5] = +Infinity;
y[6] = NaN;
var ynum = 7;

for (var i = 0; i < ynum; i++)
{
	if (!isNaN(Math.pow(x,y[i])))
	{
		$ERROR("#1: isNaN(Math.pow(" + x + ", " + y[i] + ")) === false");
	}
}
