// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of join does not have the attribute DontDelete
es5id: 15.4.4.5_A6.2
description: Checking use hasOwnProperty, delete
---*/

//CHECK#1
if (Array.prototype.join.hasOwnProperty('length') !== true) {
  $ERROR('#1: Array.prototype.join.hasOwnProperty(\'length\') === true. Actual: ' + (Array.prototype.join.hasOwnProperty('length')));
}

delete Array.prototype.join.length;
 
//CHECK#2
if (Array.prototype.join.hasOwnProperty('length') !== false) {
  $ERROR('#2: delete Array.prototype.join.length; Array.prototype.join.hasOwnProperty(\'length\') === false. Actual: ' + (Array.prototype.join.hasOwnProperty('length')));
}

//CHECK#3
if (Array.prototype.join.length === undefined) {
  $ERROR('#3: delete Array.prototype.join.length; Array.prototype.join.length !== undefined');
}
