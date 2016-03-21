// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The NaN is DontDelete
es5id: 15.1.1.2_A3_T1
description: Use delete
includes: [propertyHelper.js, fnGlobalObject.js]
---*/

// CHECK#1
verifyNotConfigurable(fnGlobalObject(), "NaN");

try {
  if (delete fnGlobalObject().NaN !== false) {
    $ERROR('#1: delete NaN === false.');
  }
} catch (e) {
  if (e instanceof Test262Error) throw e;
  assert(e instanceof TypeError);
}
