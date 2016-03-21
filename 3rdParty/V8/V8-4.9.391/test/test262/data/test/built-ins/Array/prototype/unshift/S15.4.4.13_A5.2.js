// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of unshift does not have the attribute DontDelete
es5id: 15.4.4.13_A5.2
description: Checking use hasOwnProperty, delete
---*/

//CHECK#1
if (Array.prototype.unshift.hasOwnProperty('length') !== true) {
  $ERROR('#1: Array.prototype.unshift.hasOwnProperty(\'length\') === true. Actual: ' + (Array.prototype.unshift.hasOwnProperty('length')));
}

delete Array.prototype.unshift.length;

//CHECK#2
if (Array.prototype.unshift.hasOwnProperty('length') !== false) {
  $ERROR('#2: delete Array.prototype.unshift.length; Array.prototype.unshift.hasOwnProperty(\'length\') === false. Actual: ' + (Array.prototype.unshift.hasOwnProperty('length')));
}

//CHECK#3
if (Array.prototype.unshift.length === undefined) {
  $ERROR('#3: delete Array.prototype.unshift.length; Array.prototype.unshift.length !== undefined');
}
