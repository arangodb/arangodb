// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setDate" has { DontEnum } attributes
es5id: 15.9.5.36_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setDate;
if(x === 1)
  Date.prototype.setDate = 2;
else
  Date.prototype.setDate = 1;
if (Date.prototype.setDate === x) {
  $ERROR('#1: The Date.prototype.setDate has not the attribute ReadOnly');
}
