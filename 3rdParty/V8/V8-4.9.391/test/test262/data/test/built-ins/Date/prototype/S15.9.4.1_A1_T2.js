// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date property "prototype" has { DontEnum, DontDelete, ReadOnly }
    attributes
es5id: 15.9.4.1_A1_T2
description: Checking DontDelete attribute
includes: [propertyHelper.js]
---*/

verifyNotConfigurable(Date, "prototype");

try {
  if (delete Date.prototype !== false) {
    $ERROR('#1: The Date.prototype property has the attributes DontDelete');
  }
} catch (e) {
  if (e instanceof Test262Error) throw e;
  assert(e instanceof TypeError);
}

if (!Date.hasOwnProperty('prototype')) {
  $ERROR('#2: The Date.prototype property has the attributes DontDelete');
}
