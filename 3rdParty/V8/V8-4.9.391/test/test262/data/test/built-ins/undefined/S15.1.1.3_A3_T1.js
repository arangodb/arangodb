// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The undefined is DontDelete
es5id: 15.1.1.3_A3.1
description: Use delete
includes: [propertyHelper.js, fnGlobalObject.js]
---*/

// CHECK#1
verifyNotConfigurable(fnGlobalObject(), "undefined");

try {
  if (delete fnGlobalObject().undefined !== false) {
    $ERROR('#1: delete undefined === false.');
  }
} catch (e) {
  if (e instanceof Test262Error) throw e;
  assert(e instanceof TypeError);
}
