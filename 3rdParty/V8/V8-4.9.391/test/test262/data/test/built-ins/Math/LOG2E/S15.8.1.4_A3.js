// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Value Property LOG2E of the Math Object has the attribute DontDelete
es5id: 15.8.1.4_A3
description: Checking if Math.LOG2E property has the attribute DontDelete
includes: [propertyHelper.js]
---*/

verifyNotConfigurable(Math, "LOG2E");

// CHECK#1
try {
  if (delete Math.LOG2E === true) {
    $ERROR('#1: Value Property LOG2E of the Math Object hasn\'t attribute DontDelete: \'Math.LOG2E === true\'');
  }
} catch (e) {
  if (e instanceof Test262Error) throw e;
  assert(e instanceof TypeError);
}
