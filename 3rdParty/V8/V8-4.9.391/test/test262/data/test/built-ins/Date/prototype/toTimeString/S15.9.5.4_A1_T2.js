// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "toTimeString" has { DontEnum } attributes
es5id: 15.9.5.4_A1_T2
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.toTimeString  === false) {
  $ERROR('#1: The Date.prototype.toTimeString property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('toTimeString')) {
  $ERROR('#2: The Date.prototype.toTimeString property has not the attributes DontDelete');
}
