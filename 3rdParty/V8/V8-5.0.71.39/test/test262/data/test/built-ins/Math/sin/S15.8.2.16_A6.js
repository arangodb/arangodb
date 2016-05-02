// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Sine is a periodic function with period 2*PI
es5id: 15.8.2.16_A6
description: >
    Checking if Math.sin(x) equals to Math.sin(x+n*2*Math.PI) with
    precision 0.000000000003, where n is an integer from 1 to 100 and
    x is one of 10 float point values from 0 to 2*Math.PI
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
	curval = Math.sin(x[i]);
	curx = x[i] + period;
	var j = 0;
	while (curx <= b)
	{
		curx += period;
		j++;
		if (Math.abs(Math.sin(curx) - curval) >= prec)
		{
			$ERROR("#1: sin is found out to not be periodic:\n Math.abs(Math.sin(" + x[i] + ") - Math.sin(" + x[i] + " + 2*Math.PI*" + j + ")) >= " + prec + "\n Math.sin(" + x[i] + ") === " + curval + "\n Math.sin(" + curx + ") === " + Math.sin(curx));
		}
	}
}
