// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date.prototype.toDateString property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
es5id: 15.9.5.3_A3_T1
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.toDateString.length;
verifyNotWritable(Date.prototype.toDateString, "length", null, 1);
if (Date.prototype.toDateString.length !== x) {
  $ERROR('#1: The Date.prototype.toDateString.length has the attribute ReadOnly');
}
