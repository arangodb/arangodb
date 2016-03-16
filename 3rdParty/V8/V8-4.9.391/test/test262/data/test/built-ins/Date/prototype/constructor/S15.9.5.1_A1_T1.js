// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "constructor" has { DontEnum } attributes
es5id: 15.9.5.1_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.constructor;
if(x === 1)
  Date.prototype.constructor = 2;
else
  Date.prototype.constructor = 1;
if (Date.prototype.constructor === x) {
  $ERROR('#1: The Date.prototype.constructor has not the attribute ReadOnly');
}
