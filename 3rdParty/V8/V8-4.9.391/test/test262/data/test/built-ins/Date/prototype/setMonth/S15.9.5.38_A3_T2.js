// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date.prototype.setMonth property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
es5id: 15.9.5.38_A3_T2
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.setMonth.length  !== true) {
  $ERROR('#1: The Date.prototype.setMonth.length property does not have the attributes DontDelete');
}

if (Date.prototype.setMonth.hasOwnProperty('length')) {
  $ERROR('#2: The Date.prototype.setMonth.length property does not have the attributes DontDelete');
}
