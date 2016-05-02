// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The value of the internal [[Prototype]] property of the Function
    prototype object is the Object prototype object (15.3.4)
es5id: 15.3.4_A3_T1
description: Checking prototype of Function.prototype
---*/

if (Object.getPrototypeOf(Function.prototype) !== Object.prototype) {
  $ERROR('#1: The value of the internal [[Prototype]] property of ' +
         'the Function prototype object is the Object prototype ' +
         'object (15.3.4)');
}
