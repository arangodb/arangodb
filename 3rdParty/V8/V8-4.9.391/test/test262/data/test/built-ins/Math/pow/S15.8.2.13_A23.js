// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    If x<0 and x is finite and y is finite and y is not an integer,
    Math.pow(x,y) is NaN
es5id: 15.8.2.13_A23
description: >
    Checking if Math.pow(x,y) is NaN, where x<0 and x is finite and y
    is finite and y is not an integer
---*/

// CHECK#1

var y = new Array();
var x = new Array();
x[0] = -1.7976931348623157E308; //largest (by module) finite number
x[1] = -Math.PI;
x[2] = -1;
x[3] = -0.000000000000001;
var xnum = 4;

y[0] = -Math.PI;
y[1] = -Math.E;
y[2] = -1.000000000000001;
y[3] = -0.000000000000001;
y[4] = 0.000000000000001;
y[5] = 1.000000000000001;
y[6] = Math.E;
y[7] = Math.PI;
var ynum = 8;

for (var i = 0; i < xnum; i++)
	for (var j = 0; j < ynum; j++)
		if (!isNaN(Math.pow(x[i],y[j])))
			$ERROR("#1: isNaN(Math.pow(" + x[i] + ", " + y[j] + ")) === false");
