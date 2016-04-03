// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The NaN is ReadOnly
es5id: 15.1.1.1_A2_T2
description: Checking typeof Functions
flags: [noStrict]
---*/

// CHECK#1
NaN = true;
if (typeof(NaN) === "boolean") {
	$ERROR('#1: NaN = true; typeof(NaN) !== "boolean". Actual: ' + (typeof(NaN)));
}
