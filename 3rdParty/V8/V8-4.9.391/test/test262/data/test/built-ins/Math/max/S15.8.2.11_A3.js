// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: +0 is considered to be larger than -0
es5id: 15.8.2.11_A3
description: Checking if Math.max(-0,+0) and Math.max(+0,-0) equals to +0
---*/

// CHECK#1
if (Math.max(-0, +0) !== +0)
{
	$ERROR("#1: 'Math.max(-0, +0) !== +0'");
}

// CHECK#1
if (Math.max(+0, -0) !== +0)
{
	$ERROR("#2: 'Math.max(+0, -0) !== +0'");
}
