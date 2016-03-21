// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setMilliseconds" has { DontEnum } attributes
es5id: 15.9.5.28_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setMilliseconds;
if(x === 1)
  Date.prototype.setMilliseconds = 2;
else
  Date.prototype.setMilliseconds = 1;
if (Date.prototype.setMilliseconds === x) {
  $ERROR('#1: The Date.prototype.setMilliseconds has not the attribute ReadOnly');
}
