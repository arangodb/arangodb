// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The RegExp.prototype.exec.length property has the attribute ReadOnly
es5id: 15.10.6.2_A10
description: Checking if varying the RegExp.prototype.exec.length property fails
includes: [propertyHelper.js]
---*/

//CHECK#1
if (RegExp.prototype.exec.hasOwnProperty('length') !== true) {
  $ERROR('#1: RegExp.prototype.exec.hasOwnProperty(\'length\') === true');
}

var __obj = RegExp.prototype.exec.length;

verifyNotWritable(RegExp.prototype.exec, "length", null, function(){return "shifted";});

//CHECK#2
if (RegExp.prototype.exec.length !== __obj) {
  $ERROR('#2: __obj = RegExp.prototype.exec.length; RegExp.prototype.exec.length = function(){return "shifted";}; RegExp.prototype.exec.length === __obj. Actual: ' + (RegExp.prototype.exec.length));
}
