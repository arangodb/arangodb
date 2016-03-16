// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The RegExp.prototype multiline property does not have a set accessor
es5id: 15.10.7.4_A10
description: Checking if varying the multiline property fails
includes: [propertyHelper.js]
---*/

var __re = RegExp.prototype;

//CHECK#1
if (__re.hasOwnProperty('multiline') !== true) {
  $ERROR('#1: __re = RegExp.prototype; __re.hasOwnProperty(\'multiline\') === true');
}

var __sample = /\n/;
var __obj = __sample.multiline;

verifyNotWritable(__sample, "multiline", "multiline", "shifted");

//CHECK#2
if (__sample.multiline !== __obj) {
  $ERROR('#2: __sample = /\n/; __obj = __sample.multiline; __sample.multiline = "shifted"; __sample.multiline === __obj. Actual: ' + (__sample.multiline));
}
