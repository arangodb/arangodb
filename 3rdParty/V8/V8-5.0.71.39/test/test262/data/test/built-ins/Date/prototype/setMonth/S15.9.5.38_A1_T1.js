// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setMonth" has { DontEnum } attributes
es5id: 15.9.5.38_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setMonth;
if(x === 1)
  Date.prototype.setMonth = 2;
else
  Date.prototype.setMonth = 1;
if (Date.prototype.setMonth === x) {
  $ERROR('#1: The Date.prototype.setMonth has not the attribute ReadOnly');
}
