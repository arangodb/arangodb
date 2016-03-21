// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCSeconds" has { DontEnum } attributes
es5id: 15.9.5.31_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setUTCSeconds;
if(x === 1)
  Date.prototype.setUTCSeconds = 2;
else
  Date.prototype.setUTCSeconds = 1;
if (Date.prototype.setUTCSeconds === x) {
  $ERROR('#1: The Date.prototype.setUTCSeconds has not the attribute ReadOnly');
}
