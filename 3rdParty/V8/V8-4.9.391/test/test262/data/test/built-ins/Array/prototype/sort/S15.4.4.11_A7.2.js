// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of sort does not have the attribute DontDelete
es5id: 15.4.4.11_A7.2
description: Checking use hasOwnProperty, delete
---*/

//CHECK#1
if (Array.prototype.sort.hasOwnProperty('length') !== true) {
  $ERROR('#1: Array.sort.prototype.hasOwnProperty(\'length\') === true. Actual: ' + (Array.sort.prototype.hasOwnProperty('length')));
}

delete Array.prototype.sort.length;

//CHECK#2
if (Array.prototype.sort.hasOwnProperty('length') !== false) {
  $ERROR('#2: delete Array.prototype.sort.length; Array.prototype.sort.hasOwnProperty(\'length\') === false. Actual: ' + (Array.prototype.sort.hasOwnProperty('length')));
}

//CHECK#3
if (Array.prototype.sort.length === undefined) {
  $ERROR('#3: delete Array.prototype.sort.length; Array.prototype.sort.length !== undefined');
}
