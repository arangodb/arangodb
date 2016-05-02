// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If x is NaN, Math.exp(x) is NaN
es5id: 15.8.2.8_A1
description: Checking if Math.exp(NaN) is NaN
---*/

// CHECK#1
var x = NaN;
if (!isNaN(Math.exp(x)))
{
	$ERROR("#1: 'var x=NaN; isNaN(Math.exp(x)) === false'");
}
