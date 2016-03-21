// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    If y is +0 and x is -0, Math.atan2(y,x) is an implementation-dependent
    approximation to +PI
es5id: 15.8.2.5_A6
description: >
    Checking if Math.atan2(y,x) is an approximation to +PI, where y is
    +0 and x is -0
includes:
    - math_precision.js
    - math_isequal.js
---*/

// CHECK#1
//prec = 0.00000000000001;
var y = +0;
var x = -0;
if (!isEqual(Math.atan2(y,x), Math.PI))
	$ERROR("#1: Math.abs(Math.atan2(" + y + ", -0) - Math.PI) >= " + prec);
