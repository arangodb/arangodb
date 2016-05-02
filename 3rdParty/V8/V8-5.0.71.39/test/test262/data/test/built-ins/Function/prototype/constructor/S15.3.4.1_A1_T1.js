// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The initial value of Function.prototype.constructor is the built-in
    Function constructor
es5id: 15.3.4.1_A1_T1
description: Checking Function.prototype.constructor
---*/

//CHECK#1
if (Function.prototype.constructor !== Function) {
  $ERROR('#1: The initial value of Function.prototype.constructor is the built-in Function constructor');
}
