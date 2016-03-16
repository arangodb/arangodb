// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Object.prototype.isPrototypeOf.length property has the attribute
    ReadOnly
es5id: 15.2.4.6_A10
description: >
    Checking if varying the Object.prototype.isPrototypeOf.length
    property fails
includes: [propertyHelper.js]
---*/

//CHECK#1
if (!(Object.prototype.isPrototypeOf.hasOwnProperty('length'))) {
  $ERROR('#1: the Object.prototype.isPrototypeOf has length property');
}

var obj = Object.prototype.isPrototypeOf.length;

verifyNotWritable(Object.prototype.isPrototypeOf, "length", null, function(){return "shifted";});

//CHECK#2
if (Object.prototype.isPrototypeOf.length !== obj) {
  $ERROR('#2: the Object.prototype.isPrototypeOf length property has the attributes ReadOnly');
}
