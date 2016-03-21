// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The initial value of Object.prototype.constructor is the built-in Object
    constructor
es5id: 15.2.4.1_A1_T1
description: Checking the Object.prototype.constructor
---*/

//CHECK#1
if (Object.prototype.constructor !== Object) {
  $ERROR('#1: The initial value of Object.prototype.constructor is the built-in Object constructor');
}
