// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getHours" has { DontEnum } attributes
es5id: 15.9.5.18_A1_T2
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.getHours  === false) {
  $ERROR('#1: The Date.prototype.getHours property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('getHours')) {
  $ERROR('#2: The Date.prototype.getHours property has not the attributes DontDelete');
}
