// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Function.prototype.call.length property has the attribute ReadOnly
es5id: 15.3.4.4_A10
description: >
    Checking if varying the Function.prototype.call.length property
    fails
includes: [propertyHelper.js]
---*/

//CHECK#1
if (!(Function.prototype.call.hasOwnProperty('length'))) {
  $ERROR('#1: the Function.prototype.call has length property.');
}

var obj = Function.prototype.call.length;

verifyNotWritable(Function.prototype.call, "length", null, function(){return "shifted";});

//CHECK#2
if (Function.prototype.call.length !== obj) {
  $ERROR('#2: the Function.prototype.call length property has the attributes ReadOnly.');
}
