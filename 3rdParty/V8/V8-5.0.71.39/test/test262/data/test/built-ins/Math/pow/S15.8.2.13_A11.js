// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If x is +Infinity and y>0, Math.pow(x,y) is +Infinity
es5id: 15.8.2.13_A11
description: >
    Checking if Math.pow(x,y) equals to +Infinity, where x is
    +Infinity and y>0
---*/

// CHECK#1

var x = +Infinity;
var y = new Array();
y[3] = Infinity;
y[2] = 1.7976931348623157E308; //largest (by module) finite number
y[1] = 1;
y[0] = 0.000000000000001;
var ynum = 4;

for (var i = 0; i < ynum; i++)
{
	if (Math.pow(x,y[i]) !== +Infinity)
	{
		$ERROR("#1: Math.pow(" + x + ", " + y[i] + ") !== +Infinity");
	}
}
