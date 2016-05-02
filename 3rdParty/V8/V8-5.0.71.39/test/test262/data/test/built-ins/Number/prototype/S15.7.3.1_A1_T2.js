// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Number property "prototype" has { DontEnum, DontDelete, ReadOnly }
    attributes
es5id: 15.7.3.1_A1_T2
description: Checking if deleting the Number.prototype property fails
includes: [propertyHelper.js]
---*/

verifyNotConfigurable(Number, "prototype");

// CHECK#1
try {
  if (delete Number.prototype !== false) {
    $ERROR('#1: The Number.prototype property has the attributes DontDelete');
  }
} catch (e) {
  if (e instanceof Test262Error) throw e;
  assert(e instanceof TypeError);
}

if (!Number.hasOwnProperty('prototype')) {
  $ERROR('#2: The Number.prototype property has the attributes DontDelete');
}
