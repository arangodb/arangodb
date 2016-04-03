// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Number.POSITIVE_INFINITY is DontDelete
es5id: 15.7.3.6_A3
description: Checking if deleting Number.POSITIVE_INFINITY fails
includes: [propertyHelper.js]
---*/

verifyNotConfigurable(Number, "POSITIVE_INFINITY");

// CHECK#1
try {
  if (delete Number.POSITIVE_INFINITY !== false) {
    $ERROR('#1: delete Number.POSITIVE_INFINITY === false');
  }
} catch (e) {
  if (e instanceof Test262Error) throw e;
  assert(e instanceof TypeError);
}
