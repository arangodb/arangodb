// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The RegExp.prototype global property does not have a set accessor
es5id: 15.10.7.2_A10
description: Checking if varying the global property fails
includes: [propertyHelper.js]
---*/

var __re = RegExp.prototype;

//CHECK#1
if (__re.hasOwnProperty('global') !== true) {
  $ERROR('#1: __re = RegExp.prototype; __re.hasOwnProperty(\'global\') === true');
}

var __sample = /^|^/;
var __obj = __sample.global;

verifyNotWritable(__sample, "global", "global", "shifted");

//CHECK#2
if (__sample.global !== __obj) {
  $ERROR('#2: __sample = /^|^/; __obj = __sample.global; __sample.global = "shifted"; __sample.global === __obj. Actual: ' + (__sample.global));
}
