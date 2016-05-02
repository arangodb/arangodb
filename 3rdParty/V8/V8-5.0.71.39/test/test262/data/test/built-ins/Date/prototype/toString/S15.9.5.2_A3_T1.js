// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date.prototype.toString property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
es5id: 15.9.5.2_A3_T1
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.toString.length;
verifyNotWritable(Date.prototype.toString, "length", null, 1);
if (Date.prototype.toString.length !== x) {
  $ERROR('#1: The Date.prototype.toString.length has the attribute ReadOnly');
}
