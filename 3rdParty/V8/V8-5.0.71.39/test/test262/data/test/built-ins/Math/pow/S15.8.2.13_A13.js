// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    If x is -Infinity and y>0 and y is an odd integer, Math.pow(x,y) is
    -Infinity
es5id: 15.8.2.13_A13
description: >
    Checking if Math.pow(x,y) equals to -Infinity, where x is
    -Infinity and y>0
---*/

// CHECK#1

var x = -Infinity;
var y = new Array();
y[0] = 1;
y[1] = 111;
y[2] = 111111;
var ynum = 3;

for (var i = 0; i < ynum; i++)
{
	if (Math.pow(x,y[i]) !== -Infinity)
	{
		$ERROR("#1: Math.pow(" + x + ", " + y[i] + ") !== -Infinity");
	}
}
