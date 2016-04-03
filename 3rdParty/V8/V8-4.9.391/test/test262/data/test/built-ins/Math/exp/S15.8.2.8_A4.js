// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If x is +Infinity, Math.exp(x) is +Ifinity
es5id: 15.8.2.8_A4
description: Checking if Math.exp(+Infinity) is +Ifinity
---*/

// CHECK#1
var x = +Infinity;
if (Math.exp(x) !== +Infinity)
{
	$ERROR("#1: 'var x = +Infinity; Math.exp(x) !== +Infinity'");
}
