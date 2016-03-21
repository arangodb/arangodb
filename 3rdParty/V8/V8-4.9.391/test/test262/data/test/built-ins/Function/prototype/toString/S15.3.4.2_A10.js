// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Function.prototype.toString.length property has the attribute ReadOnly
es5id: 15.3.4.2_A10
description: >
    Checking if varying the Function.prototype.toString.length
    property fails
includes: [propertyHelper.js]
---*/

//CHECK#1
if (!(Function.prototype.toString.hasOwnProperty('length'))) {
  $ERROR('#1: the Function.prototype.toString has length property.');
}

var obj = Function.prototype.toString.length;

verifyNotWritable(Function.prototype.toString, "length", null, function(){return "shifted";});

//CHECK#2
if (Function.prototype.toString.length !== obj) {
  $ERROR('#2: the Function.prototype.toString length property has the attributes ReadOnly.');
}
