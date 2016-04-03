// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date.prototype.setUTCSeconds property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
es5id: 15.9.5.31_A3_T2
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.setUTCSeconds.length  !== true) {
  $ERROR('#1: The Date.prototype.setUTCSeconds.length property does not have the attributes DontDelete');
}

if (Date.prototype.setUTCSeconds.hasOwnProperty('length')) {
  $ERROR('#2: The Date.prototype.setUTCSeconds.length property does not have the attributes DontDelete');
}
