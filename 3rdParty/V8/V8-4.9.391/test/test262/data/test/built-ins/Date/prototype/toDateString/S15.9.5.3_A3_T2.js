// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date.prototype.toDateString property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
es5id: 15.9.5.3_A3_T2
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.toDateString.length  !== true) {
  $ERROR('#1: The Date.prototype.toDateString.length property does not have the attributes DontDelete');
}

if (Date.prototype.toDateString.hasOwnProperty('length')) {
  $ERROR('#2: The Date.prototype.toDateString.length property does not have the attributes DontDelete');
}
