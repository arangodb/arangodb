// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If x is NaN, Math.sqrt(x) is NaN
es5id: 15.8.2.17_A1
description: Checking if Math.sqrt(NaN) is NaN
---*/

// CHECK#1
var x = NaN;
if (!isNaN(Math.sqrt(x)))
{
	$ERROR("#1: 'var x=NaN; isNaN(Math.sqrt(x)) === false'");
}
