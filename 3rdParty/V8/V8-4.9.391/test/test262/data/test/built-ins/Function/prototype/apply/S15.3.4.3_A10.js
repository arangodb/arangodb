// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Function.prototype.apply.length property has the attribute ReadOnly
es5id: 15.3.4.3_A10
description: >
    Checking if varying the Function.prototype.apply.length property
    fails
includes: [propertyHelper.js]
---*/

//CHECK#1
if (!(Function.prototype.apply.hasOwnProperty('length'))) {
  $ERROR('#1: the Function.prototype.apply has length property.');
}

var obj = Function.prototype.apply.length;

verifyNotWritable(Function.prototype.apply, "length", null, function(){return "shifted";});

//CHECK#2
if (Function.prototype.apply.length !== obj) {
  $ERROR('#2: the Function.prototype.apply length property has the attributes ReadOnly.');
}
