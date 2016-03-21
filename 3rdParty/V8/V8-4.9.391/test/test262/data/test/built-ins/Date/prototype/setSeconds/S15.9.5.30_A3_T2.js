// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date.prototype.setSeconds property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
es5id: 15.9.5.30_A3_T2
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.setSeconds.length  !== true) {
  $ERROR('#1: The Date.prototype.setSeconds.length property does not have the attributes DontDelete');
}

if (Date.prototype.setSeconds.hasOwnProperty('length')) {
  $ERROR('#2: The Date.prototype.setSeconds.length property does not have the attributes DontDelete');
}
