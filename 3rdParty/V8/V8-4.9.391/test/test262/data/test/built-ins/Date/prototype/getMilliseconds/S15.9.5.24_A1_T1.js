// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getMilliseconds" has { DontEnum } attributes
es5id: 15.9.5.24_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getMilliseconds;
if(x === 1)
  Date.prototype.getMilliseconds = 2;
else
  Date.prototype.getMilliseconds = 1;
if (Date.prototype.getMilliseconds === x) {
  $ERROR('#1: The Date.prototype.getMilliseconds has not the attribute ReadOnly');
}
