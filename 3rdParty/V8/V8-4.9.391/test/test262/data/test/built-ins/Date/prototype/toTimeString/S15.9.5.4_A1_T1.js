// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "toTimeString" has { DontEnum } attributes
es5id: 15.9.5.4_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.toTimeString;
if(x === 1)
  Date.prototype.toTimeString = 2;
else
  Date.prototype.toTimeString = 1;
if (Date.prototype.toTimeString === x) {
  $ERROR('#1: The Date.prototype.toTimeString has not the attribute ReadOnly');
}
