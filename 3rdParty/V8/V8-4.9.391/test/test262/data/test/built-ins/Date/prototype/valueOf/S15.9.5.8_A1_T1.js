// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "valueOf" has { DontEnum } attributes
es5id: 15.9.5.8_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.valueOf;
if(x === 1)
  Date.prototype.valueOf = 2;
else
  Date.prototype.valueOf = 1;
if (Date.prototype.valueOf === x) {
  $ERROR('#1: The Date.prototype.valueOf has not the attribute ReadOnly');
}
