// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If x is -0 and y<0 and y is an odd integer, Math.pow(x,y) is -Infinity
es5id: 15.8.2.13_A21
description: >
    Checking if Math.pow(x,y) equals to -Infinity, where x is -0 and y
    is an odd integer
---*/

// CHECK#1

var x = -0;
var y = new Array();
y[2] = -1;
y[1] = -111; 
y[0] = -111111;
var ynum = 3;

for (var i = 0; i < ynum; i++)
{
	if (Math.pow(x,y[i]) !== -Infinity)
	{
		$ERROR("#1: Math.pow(" + x + ", " + y[i] + ") !== -Infinity");
	}
}
