// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date.prototype.getHours property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
es5id: 15.9.5.18_A3_T1
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getHours.length;
verifyNotWritable(Date.prototype.getHours, "length", null, 1);
if (Date.prototype.getHours.length !== x) {
  $ERROR('#1: The Date.prototype.getHours.length has the attribute ReadOnly');
}
