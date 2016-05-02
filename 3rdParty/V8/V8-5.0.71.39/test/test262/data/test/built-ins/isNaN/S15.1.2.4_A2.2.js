// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of isNaN does not have the attribute DontDelete
es5id: 15.1.2.4_A2.2
description: Checking use hasOwnProperty, delete
---*/

//CHECK#1
if (isNaN.hasOwnProperty('length') !== true) {
  $ERROR('#1: isNaN.hasOwnProperty(\'length\') === true. Actual: ' + (isNaN.hasOwnProperty('length')));
}

delete isNaN.length;

//CHECK#2
if (isNaN.hasOwnProperty('length') !== false) {
  $ERROR('#2: delete isNaN.length; isNaN.hasOwnProperty(\'length\') === false. Actual: ' + (isNaN.hasOwnProperty('length')));
}

//CHECK#3
if (isNaN.length === undefined) {
  $ERROR('#3: delete isNaN.length; isNaN.length !== undefined');
}
