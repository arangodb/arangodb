// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setTime" has { DontEnum } attributes
es5id: 15.9.5.27_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setTime;
if(x === 1)
  Date.prototype.setTime = 2;
else
  Date.prototype.setTime = 1;
if (Date.prototype.setTime === x) {
  $ERROR('#1: The Date.prototype.setTime has not the attribute ReadOnly');
}
