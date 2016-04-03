// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date.prototype.getUTCDate property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
es5id: 15.9.5.15_A3_T1
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getUTCDate.length;
verifyNotWritable(Date.prototype.getUTCDate, "length", null, 1);
if (Date.prototype.getUTCDate.length !== x) {
  $ERROR('#1: The Date.prototype.getUTCDate.length has the attribute ReadOnly');
}
