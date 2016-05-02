// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of shift does not have the attribute DontDelete
es5id: 15.4.4.9_A5.2
description: Checking use hasOwnProperty, delete
---*/

//CHECK#1
if (Array.prototype.shift.hasOwnProperty('length') !== true) {
  $ERROR('#1: Array.prototype.shift.hasOwnProperty(\'length\') === true. Actual: ' + (Array.prototype.shift.hasOwnProperty('length')));
}

delete Array.prototype.shift.length;

//CHECK#2
if (Array.prototype.shift.hasOwnProperty('length') !== false) {
  $ERROR('#2: delete Array.prototype.shift.length; Array.prototype.shift.hasOwnProperty(\'length\') === false. Actual: ' + (Array.prototype.shift.hasOwnProperty('length')));
}

//CHECK#3
if (Array.prototype.shift.length === undefined) {
  $ERROR('#3: delete Array.prototype.shift.length; Array.prototype.shift.length !== undefined');
}
