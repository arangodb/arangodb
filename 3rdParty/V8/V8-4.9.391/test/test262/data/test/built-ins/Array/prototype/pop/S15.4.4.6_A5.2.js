// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of pop does not have the attribute DontDelete
es5id: 15.4.4.6_A5.2
description: Checking use hasOwnProperty, delete
---*/

//CHECK#1
if (Array.prototype.pop.hasOwnProperty('length') !== true) {
  $ERROR('#1: Array.prototype.pop.hasOwnProperty(\'length\') === true. Actual: ' + (Array.prototype.pop.hasOwnProperty('length')));
}

delete Array.prototype.pop.length;

//CHECK#2
if (Array.prototype.pop.hasOwnProperty('length') !== false) {
  $ERROR('#2: delete Array.prototype.pop.length; Array.prototype.pop.hasOwnProperty(\'length\') === false. Actual: ' + (Array.prototype.pop.hasOwnProperty('length')));
}

//CHECK#3
if (Array.prototype.pop.length === undefined) {
  $ERROR('#3: delete Array.prototype.pop.length; Array.prototype.pop.length !== undefined');
}
