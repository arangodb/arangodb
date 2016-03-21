// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The length property of toLocaleString does not have the attribute
    DontDelete
es5id: 15.4.4.3_A4.2
description: Checking use hasOwnProperty, delete
---*/

//CHECK#1
if (Array.prototype.toLocaleString.hasOwnProperty('length') !== true) {
  $ERROR('#1: Array.prototype.toLocaleString.hasOwnProperty(\'length\') === true. Actual: ' + (Array.prototype.toLocaleString.hasOwnProperty('length')));
}

delete Array.prototype.toLocaleString.length;

//CHECK#2
if (Array.prototype.toLocaleString.hasOwnProperty('length') !== false) {
  $ERROR('#2: delete Array.prototype.toLocaleString.length; Array.prototype.toLocaleString.hasOwnProperty(\'length\') === false. Actual: ' + (Array.prototype.toLocaleString.hasOwnProperty('length')));
}

//CHECK#3
if (Array.prototype.toLocaleString.length === undefined) {
  $ERROR('#3: delete Array.prototype.toLocaleString.length; Array.prototype.toLocaleString.length !== undefined');
}
