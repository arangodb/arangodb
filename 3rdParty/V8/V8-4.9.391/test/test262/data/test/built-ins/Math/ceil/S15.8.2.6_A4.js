// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If x is +Infinity, Math.ceil(x) is +Infinity
es5id: 15.8.2.6_A4
description: Checking if Math.ceil(x) is +Infinity, where x is +Infinity
---*/

// CHECK#1
var x = +Infinity;
if (Math.ceil(x) !== +Infinity)
{
	$ERROR("#1: 'var x = +Infinity; Math.ceil(x) !== +Infinity'");
}
