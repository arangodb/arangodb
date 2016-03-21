// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Cosine is a periodic function with period 2*PI
es5id: 15.8.2.7_A6
description: >
    Checking if Math.cos(x) equals to Math.cos(x+n*2*Math.PI) with
    precision 0.000000000003, where n is an integer from 1 to 100 and
    x is one of 10 float point values from -Math.PI to +Math.PI
---*/

// CHECK#1
var prec = 0.000000000003;
//prec = 0.000000000000001;
var period = 2*Math.PI;
var pernum = 100;

var a = -pernum * period;
var b = pernum * period;
var snum = 9;
var step = period/snum + 0.0;
var x = new Array();
for (var i = 0; i < snum; i++)
{
	x[i] = a + i*step;
}
x[9] = a + period;

var curval;
var curx;
var j;
for (i = 0; i < snum; i++)
{
	curval = Math.cos(x[i]);
	curx = x[i] + period;
	j = 0;
	while (curx <= b)
	{
		curx += period;
		j++;
		if (Math.abs(Math.cos(curx) - curval) >= prec)
		{
			$ERROR("#1: cos is found out to not be periodic:\n Math.abs(Math.cos(" + x[i] + ") - Math.cos(" + x[i] + " + 2*Math.PI*" + j + ")) >= " + prec + "\n Math.cos(" + x[i] + ") === " + curval + "\n Math.cos(" + curx + ") === " + Math.cos(curx));
		}
	}
}
