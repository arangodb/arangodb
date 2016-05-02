// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If x is equal to -0, Math.sqrt(x) is -0
es5id: 15.8.2.17_A4
description: Checking if Math.sqrt(-0) equals to -0
---*/

// CHECK#1
var x = -0;
if (Math.sqrt(x) !== -0)
{
	$ERROR("#1: 'var x=-0; Math.sqrt(x) !== -0'");
}
