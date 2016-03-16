// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Tangent is a periodic function with period PI
es5id: 15.8.2.18_A6
description: >
    Checking if Math.tan(x) equals to Math.tan(x+n*Math.PI) with
    precision 0.000000000003, where n is an integer from 1 to 100 and
    x is one of 10 float point values from 0 to Math.PI
---*/

// CHECK#1
var prec = 0.00000000003;
//prec = 0.000000000000001;
var period = Math.PI;
var pernum = 100;

var a = -pernum * period + period/2;
var b = pernum * period - period/2;
var snum = 9;
var step = period/(snum + 2);
var x = new Array();
for (var i = 0; i <= snum; i++)    //// We exlude special points
{							   ////           in this cycle.
	x[i] = a + (i+1)*step;     ////
}							   ////


var curval;
var curx;
var j;
for (i = 0; i < snum; i++)
{
	curval = Math.tan(x[i]);
	curx = x[i] + period;
	var j = 0;
	while (curx <= b)
	{
		curx += period;
		j++;
		if (Math.abs(Math.tan(curx) - curval) >= prec)
		{
			$ERROR("#1: tan is found out to not be periodic:\n Math.abs(Math.tan(" + x[i] + ") - Math.tan(" + x[i] + " + 2*Math.PI*" + j + ")) >= " + prec + "\n Math.tan(" + x[i] + ") === " + curval + "\n Math.tan(" + curx + ") === " + Math.tan(curx));
		}
	}
}
