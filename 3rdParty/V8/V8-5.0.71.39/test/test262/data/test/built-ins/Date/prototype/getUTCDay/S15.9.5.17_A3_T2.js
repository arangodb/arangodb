// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date.prototype.getUTCDay property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
es5id: 15.9.5.17_A3_T2
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.getUTCDay.length  !== true) {
  $ERROR('#1: The Date.prototype.getUTCDay.length property does not have the attributes DontDelete');
}

if (Date.prototype.getUTCDay.hasOwnProperty('length')) {
  $ERROR('#2: The Date.prototype.getUTCDay.length property does not have the attributes DontDelete');
}
