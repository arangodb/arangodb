// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date.prototype.getUTCMilliseconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
es5id: 15.9.5.25_A3_T1
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getUTCMilliseconds.length;
verifyNotWritable(Date.prototype.getUTCMilliseconds, "length", null, 1);
if (Date.prototype.getUTCMilliseconds.length !== x) {
  $ERROR('#1: The Date.prototype.getUTCMilliseconds.length has the attribute ReadOnly');
}
