// Copyright 2011 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The toString function is not generic; it throws a TypeError exception if
    its this value is not a Function object.
es5id: 15.3.4.2_A15
description: >
    Whether or not they are callable, RegExp objects are not Function
    objects, so toString should throw a TypeError.
---*/

assert.throws(TypeError, function() {
  Function.prototype.toString.call(/x/);
});
