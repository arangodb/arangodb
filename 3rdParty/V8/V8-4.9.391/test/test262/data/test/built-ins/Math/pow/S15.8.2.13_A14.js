// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    If x is -Infinity and y>0 and y is NOT an odd integer, Math.pow(x,y) is
    +Infinity
es5id: 15.8.2.13_A14
description: >
    Checking if Math.pow(x,y) equals to +Infinity, where x is
    -Infinity and y>0
---*/

// CHECK#1

var x = -Infinity;
var y = new Array();
y[0] = 0.000000000000001;
y[1] = 2;
y[2] = Math.PI;
y[3] = 1.7976931348623157E308; //largest finite number
y[4] = +Infinity;
var ynum = 5;

for (var i = 0; i < ynum; i++)
{
	if (Math.pow(x,y[i]) !== +Infinity)
	{
		$ERROR("#1: Math.pow(" + x + ", " + y[i] + ") !== +Infinity");
	}
}
