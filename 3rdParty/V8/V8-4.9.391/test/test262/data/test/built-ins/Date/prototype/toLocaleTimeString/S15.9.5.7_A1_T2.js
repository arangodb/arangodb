// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date.prototype property "toLocaleTimeString" has { DontEnum }
    attributes
es5id: 15.9.5.7_A1_T2
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.toLocaleTimeString  === false) {
  $ERROR('#1: The Date.prototype.toLocaleTimeString property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('toLocaleTimeString')) {
  $ERROR('#2: The Date.prototype.toLocaleTimeString property has not the attributes DontDelete');
}
