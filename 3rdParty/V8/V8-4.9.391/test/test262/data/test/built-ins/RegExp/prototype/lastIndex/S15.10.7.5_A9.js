// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The RegExp instance lastIndex property has the attribute DontDelete
es5id: 15.10.7.5_A9
description: Checking if deleting the lastIndex property fails
includes: [propertyHelper.js]
---*/

var __re = new RegExp;

//CHECK#0
if (__re.hasOwnProperty('lastIndex') !== true) {
  $ERROR('#0: __re = new RegExp; __re.hasOwnProperty(\'lastIndex\') === true');
}

verifyNotConfigurable(__re, "lastIndex");

//CHECK#1
try {
  if ((delete __re.lastIndex) !== false) {
    $ERROR('#1: __re = new RegExp; (delete __re.lastIndex) === false');
  }
} catch (e) {
  if (e instanceof Test262Error) throw e;
  assert(e instanceof TypeError);
}

//CHECK#2
if (__re.hasOwnProperty('lastIndex') !== true) {
  $ERROR('#2: __re = new RegExp;delete __re.lastIndex === true; __re.hasOwnProperty(\'lastIndex\') === true');
}
