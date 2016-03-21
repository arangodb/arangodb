// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date.parse property "length" has { ReadOnly, ! DontDelete, DontEnum }
    attributes
es5id: 15.9.4.2_A3_T2
description: Checking DontDelete attribute
---*/

if (delete Date.parse.length  !== true) {
  $ERROR('#1: The Date.parse.length property does not have the attributes DontDelete');
}

if (Date.parse.hasOwnProperty('length')) {
  $ERROR('#2: The Date.parse.length property does not have the attributes DontDelete');
}
