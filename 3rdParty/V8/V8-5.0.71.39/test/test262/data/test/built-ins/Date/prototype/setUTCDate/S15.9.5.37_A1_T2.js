// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCDate" has { DontEnum } attributes
es5id: 15.9.5.37_A1_T2
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.setUTCDate  === false) {
  $ERROR('#1: The Date.prototype.setUTCDate property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('setUTCDate')) {
  $ERROR('#2: The Date.prototype.setUTCDate property has not the attributes DontDelete');
}
