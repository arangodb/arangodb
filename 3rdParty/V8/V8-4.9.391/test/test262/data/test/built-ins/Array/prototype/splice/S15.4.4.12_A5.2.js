// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of splice does not have the attribute DontDelete
es5id: 15.4.4.12_A5.2
description: Checking use hasOwnProperty, delete
---*/

//CHECK#1
if (Array.prototype.splice.hasOwnProperty('length') !== true) {
  $ERROR('#1: Array.prototype.splice.hasOwnProperty(\'length\') === true. Actual: ' + (Array.prototype.splice.hasOwnProperty('length')));
}

delete Array.prototype.splice.length;

//CHECK#2
if (Array.prototype.splice.hasOwnProperty('length') !== false) {
  $ERROR('#2: delete Array.prototype.splice.length; Array.prototype.splice.hasOwnProperty(\'length\') === false. Actual: ' + (Array.prototype.splice.hasOwnProperty('length')));
}

//CHECK#3
if (Array.prototype.splice.length === undefined) {
  $ERROR('#3: delete Array.prototype.splice.length; Array.prototype.splice.length !== undefined');
}
