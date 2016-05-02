// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If x is +Infinity, Math.floor(x) is +Infinity
es5id: 15.8.2.9_A4
description: Checking if Math.floor(x) is +Infinity, where x is +Infinity
---*/

// CHECK#1
var x = +Infinity;
if (Math.floor(x) !== +Infinity)
{
	$ERROR("#1: 'var x = +Infinity; Math.floor(x) !== +Infinity'");
}
