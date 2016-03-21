// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCMonth" has { DontEnum } attributes
es5id: 15.9.5.13_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getUTCMonth;
if(x === 1)
  Date.prototype.getUTCMonth = 2;
else
  Date.prototype.getUTCMonth = 1;
if (Date.prototype.getUTCMonth === x) {
  $ERROR('#1: The Date.prototype.getUTCMonth has not the attribute ReadOnly');
}
