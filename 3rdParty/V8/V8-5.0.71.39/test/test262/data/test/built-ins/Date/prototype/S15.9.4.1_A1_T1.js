// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date property "prototype" has { DontEnum, DontDelete, ReadOnly }
    attributes
es5id: 15.9.4.1_A1_T1
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype;
verifyNotWritable(Date, "prototype", null, 1);
if (Date.prototype !== x) {
  $ERROR('#1: The Date.prototype has the attribute ReadOnly');
}
