// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date.prototype.setMinutes property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
es5id: 15.9.5.32_A3_T2
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.setMinutes.length  !== true) {
  $ERROR('#1: The Date.prototype.setMinutes.length property does not have the attributes DontDelete');
}

if (Date.prototype.setMinutes.hasOwnProperty('length')) {
  $ERROR('#2: The Date.prototype.setMinutes.length property does not have the attributes DontDelete');
}
