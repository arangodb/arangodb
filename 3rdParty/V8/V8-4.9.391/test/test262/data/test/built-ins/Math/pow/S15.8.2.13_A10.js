// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If abs(x)<1 and y is -Infinity, Math.pow(x,y) is +Infinity
es5id: 15.8.2.13_A10
description: >
    Checking if Math.pow(x,y) equals to +Infinity, where abs(x)<1 and
    y is -Infinity
---*/

// CHECK#1

var y = -Infinity;
var x = new Array();
x[0] = 0.999999999999999;
x[1] = 0.5;
x[2] = +0;
x[3] = -0;
x[4] = -0.5;
x[5] = -0.999999999999999;
var xnum = 6;

for (var i = 0; i < xnum; i++)
{
	if (Math.pow(x[i],y) !== +Infinity)
	{
		$ERROR("#1: Math.pow(" + x[i] + ", " + y + ") !== +Infinity");
	}
}
