// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of concat does not have the attribute DontDelete
es5id: 15.4.4.4_A4.2
description: Checking use hasOwnProperty, delete
---*/

//CHECK#1
if (Array.prototype.concat.hasOwnProperty('length') !== true) {
  $ERROR('#1: Array.prototype.concat.hasOwnProperty(\'length\') === true. Actual: ' + (Array.prototype.concat.hasOwnProperty('length')));
}

delete Array.prototype.concat.length;

//CHECK#2
if (Array.prototype.concat.hasOwnProperty('length') !== false) {
  $ERROR('#2: delete Array.prototype.concat.length; Array.prototype.concat.hasOwnProperty(\'length\') === false. Actual: ' + (Array.prototype.concat.hasOwnProperty('length')));
}

//CHECK#3
if (Array.prototype.concat.length === undefined) {
  $ERROR('#3: delete Array.prototype.concat.length; Array.prototype.concat.length !== undefined');
}
