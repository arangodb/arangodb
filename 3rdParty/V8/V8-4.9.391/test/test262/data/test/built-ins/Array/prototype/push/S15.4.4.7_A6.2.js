// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of push does not have the attribute DontDelete
es5id: 15.4.4.7_A6.2
description: Checking use hasOwnProperty, delete
---*/

//CHECK#1
if (Array.prototype.push.hasOwnProperty('length') !== true) {
  $ERROR('#1: Array.prototype.push.hasOwnProperty(\'length\') === true. Actual: ' + (Array.prototype.push.hasOwnProperty('length')));
}

delete Array.prototype.push.length;

//CHECK#2
if (Array.prototype.push.hasOwnProperty('length') !== false) {
  $ERROR('#2: delete Array.prototype.push.length; Array.prototype.push.hasOwnProperty(\'length\') === false. Actual: ' + (Array.prototype.push.hasOwnProperty('length')));
}

//CHECK#3
if (Array.prototype.push.length === undefined) {
  $ERROR('#3: delete Array.prototype.push.length; Array.prototype.push.length !== undefined');
}
