// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getDate" has { DontEnum } attributes
es5id: 15.9.5.14_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getDate;
if(x === 1)
  Date.prototype.getDate = 2;
else
  Date.prototype.getDate = 1;
if (Date.prototype.getDate === x) {
  $ERROR('#1: The Date.prototype.getDate has not the attribute ReadOnly');
}
