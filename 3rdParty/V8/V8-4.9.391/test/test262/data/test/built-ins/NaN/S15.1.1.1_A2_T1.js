// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The NaN is ReadOnly
es5id: 15.1.1.1_A2_T1
description: Checking typeof Functions
includes: [propertyHelper.js, fnGlobalObject.js]
---*/

// CHECK#1
verifyNotWritable(fnGlobalObject(), "NaN", null, true);
if (typeof(NaN) === "boolean") {
	$ERROR('#1: NaN = true; typeof(NaN) !== "boolean". Actual: ' + (typeof(NaN)));
}
