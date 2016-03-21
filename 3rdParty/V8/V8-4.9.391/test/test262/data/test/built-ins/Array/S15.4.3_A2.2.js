// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of Array does not have the attribute DontDelete
es5id: 15.4.3_A2.2
description: Checking use hasOwnProperty, delete
---*/

//CHECK#1
if (Array.hasOwnProperty('length') !== true) {
  $ERROR('#1: Array.hasOwnProperty(\'length\') === true. Actual: ' + (Array.hasOwnProperty('length')));
}

delete Array.length;

//CHECK#2
if (Array.hasOwnProperty('length') !== false) {
  $ERROR('#2: delete Array.length; Array.hasOwnProperty(\'length\') === false. Actual: ' + (Array.hasOwnProperty('length')));
}

//CHECK#3
if (Array.length === undefined) {
  $ERROR('#3: delete Array.length; Array.length !== undefined');
}
