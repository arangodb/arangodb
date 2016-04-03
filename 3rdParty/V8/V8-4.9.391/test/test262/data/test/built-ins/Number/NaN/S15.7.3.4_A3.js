// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Number.NaN is DontDelete
es5id: 15.7.3.4_A3
description: Checking if deleting Number.NaN fails
includes: [propertyHelper.js]
---*/

verifyNotConfigurable(Number, "NaN");

// CHECK#1
try {
  if (delete Number.NaN !== false) {
    $ERROR('#1: delete Number.NaN === false');
  }
} catch (e) {
  if (e instanceof Test262Error) throw e;
  assert(e instanceof TypeError);
}
