// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of toString does not have the attribute DontDelete
es5id: 15.4.4.2_A4.2
description: Checking use hasOwnProperty, delete
---*/

//CHECK#1
if (Array.prototype.toString.hasOwnProperty('length') !== true) {
  $ERROR('#1: Array.prototype.toString.hasOwnProperty(\'length\') === true. Actual: ' + (Array.prototype.toString.hasOwnProperty('length')));
}

delete Array.prototype.toString.length;

//CHECK#2
if (Array.prototype.toString.hasOwnProperty('length') !== false) {
  $ERROR('#2: delete Array.prototype.toString.length; Array.prototype.toString.hasOwnProperty(\'length\') === false. Actual: ' + (Array.prototype.toString.hasOwnProperty('length')));
}

//CHECK#3
if (Array.prototype.toString.length === undefined) {
  $ERROR('#3: delete Array.prototype.toString.length; Array.prototype.toString.length !== undefined');
}
