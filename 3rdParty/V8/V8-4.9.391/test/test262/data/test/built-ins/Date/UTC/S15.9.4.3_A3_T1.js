// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date.UTC property "length" has { ReadOnly, DontDelete, DontEnum }
    attributes
es5id: 15.9.4.3_A3_T1
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.UTC.length;
verifyNotWritable(Date.UTC, "length", null, 1);
if (Date.UTC.length !== x) {
  $ERROR('#1: The Date.UTC.length has the attribute ReadOnly');
}
