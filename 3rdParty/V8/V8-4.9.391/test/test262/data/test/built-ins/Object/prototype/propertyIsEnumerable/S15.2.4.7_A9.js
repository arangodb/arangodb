// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Object.prototype.propertyIsEnumerable.length property does not have
    the attribute DontDelete
es5id: 15.2.4.7_A9
description: >
    Checking if deleting the
    Object.prototype.propertyIsEnumerable.length property fails
---*/

//CHECK#0
if (!(Object.prototype.propertyIsEnumerable.hasOwnProperty('length'))) {
  $ERROR('#0: the Object.prototype.propertyIsEnumerable has length property');
}

//CHECK#1
if (!delete Object.prototype.propertyIsEnumerable.length) {
  $ERROR('#1: The Object.prototype.propertyIsEnumerable.length property does not have the attributes DontDelete');
}
//
