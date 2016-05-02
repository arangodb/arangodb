// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Value Property E of the Math Object has the attribute DontDelete
es5id: 15.8.1.1_A3
description: Checking if Math.E property has the attribute DontDelete
includes: [propertyHelper.js]
---*/

verifyNotConfigurable(Math, "E");

// CHECK#1
try {
  if (delete Math.E === true) {
    $ERROR('#1: Value Property E of the Math Object hasn\'t attribute DontDelete: \'Math.E === true\'');
  }
} catch (e) {
  if (e instanceof Test262Error) throw e;
  assert(e instanceof TypeError);
}
