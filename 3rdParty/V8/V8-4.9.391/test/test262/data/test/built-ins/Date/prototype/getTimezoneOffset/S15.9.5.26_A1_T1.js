// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date.prototype property "getTimezoneOffset" has { DontEnum }
    attributes
es5id: 15.9.5.26_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getTimezoneOffset;
if(x === 1)
  Date.prototype.getTimezoneOffset = 2;
else
  Date.prototype.getTimezoneOffset = 1;
if (Date.prototype.getTimezoneOffset === x) {
  $ERROR('#1: The Date.prototype.getTimezoneOffset has not the attribute ReadOnly');
}
