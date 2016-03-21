// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    If y is equal to -Infinity and x is equal to -Infinity, Math.atan2(y,x)
    is an implementation-dependent approximation to -3*PI/4
es5id: 15.8.2.5_A23
description: >
    Checking if Math.atan2(y,x) is an approximation to -3*PI/4, where
    y is equal to -Infinity and x is equal to -Infinity
includes:
    - math_precision.js
    - math_isequal.js
---*/

// CHECK#1
//prec = 0.00000000000001;
var y = -Infinity;
var x = -Infinity;

if (!isEqual(Math.atan2(y,x), -(3*Math.PI)/4))
	$ERROR("#1: Math.abs(Math.atan2(" + y + ", " + x + ") + (3*Math.PI/4)) >= " + prec);
