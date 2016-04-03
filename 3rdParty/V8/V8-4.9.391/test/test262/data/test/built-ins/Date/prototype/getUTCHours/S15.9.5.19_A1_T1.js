// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCHours" has { DontEnum } attributes
es5id: 15.9.5.19_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getUTCHours;
if(x === 1)
  Date.prototype.getUTCHours = 2;
else
  Date.prototype.getUTCHours = 1;
if (Date.prototype.getUTCHours === x) {
  $ERROR('#1: The Date.prototype.getUTCHours has not the attribute ReadOnly');
}
