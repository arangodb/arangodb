// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The RegExp.prototype.test.length property has the attribute ReadOnly
es5id: 15.10.6.3_A10
description: Checking if varying the RegExp.prototype.test.length property fails
includes: [propertyHelper.js]
---*/

//CHECK#1
if (RegExp.prototype.test.hasOwnProperty('length') !== true) {
  $ERROR('#1: RegExp.prototype.test.hasOwnProperty(\'length\') === true');
}

var __obj = RegExp.prototype.test.length;

verifyNotWritable(RegExp.prototype.test, "length", null, function(){return "shifted";});

//CHECK#2
if (RegExp.prototype.test.length !== __obj) {
  $ERROR('#2: __obj = RegExp.prototype.test.length; RegExp.prototype.test.length = function(){return "shifted";}; RegExp.prototype.test.length === __obj. Actual: ' + (RegExp.prototype.test.length));
}
