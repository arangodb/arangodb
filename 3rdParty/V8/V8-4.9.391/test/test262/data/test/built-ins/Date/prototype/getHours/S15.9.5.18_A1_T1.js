// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getHours" has { DontEnum } attributes
es5id: 15.9.5.18_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getHours;
if(x === 1)
  Date.prototype.getHours = 2;
else
  Date.prototype.getHours = 1;
if (Date.prototype.getHours === x) {
  $ERROR('#1: The Date.prototype.getHours has not the attribute ReadOnly');
}
