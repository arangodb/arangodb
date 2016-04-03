// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getSeconds" has { DontEnum } attributes
es5id: 15.9.5.22_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getSeconds;
if(x === 1)
  Date.prototype.getSeconds = 2;
else
  Date.prototype.getSeconds = 1;
if (Date.prototype.getSeconds === x) {
  $ERROR('#1: The Date.prototype.getSeconds has not the attribute ReadOnly');
}
