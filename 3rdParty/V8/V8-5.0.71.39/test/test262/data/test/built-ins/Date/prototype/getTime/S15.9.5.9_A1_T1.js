// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getTime" has { DontEnum } attributes
es5id: 15.9.5.9_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getTime;
if(x === 1)
  Date.prototype.getTime = 2;
else
  Date.prototype.getTime = 1;
if (Date.prototype.getTime === x) {
  $ERROR('#1: The Date.prototype.getTime has not the attribute ReadOnly');
}
