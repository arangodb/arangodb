// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If x is -Infinity and y<0 and y is NOT an odd integer, Math.pow(x,y) is +0
es5id: 15.8.2.13_A16
description: >
    Checking if Math.pow(x,y) equals to +0, where x is -Infinity and
    y<0
---*/

// CHECK#1

var x = -Infinity;
var y = new Array();
y[4] = -0.000000000000001;
y[3] = -2;
y[2] = -Math.PI;
y[1] = -1.7976931348623157E308; //largest (by module) finite number
y[0] = -Infinity;
var ynum = 5;

for (var i = 0; i < ynum; i++)
{
	if (Math.pow(x,y[i]) !== +0)
	{
		$ERROR("#1: Math.pow(" + x + ", " + y[i] + ") !== +0");
	}
}
