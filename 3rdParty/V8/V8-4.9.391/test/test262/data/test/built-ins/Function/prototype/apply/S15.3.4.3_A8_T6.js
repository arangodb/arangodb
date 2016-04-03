// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Function.prototype.apply can`t be used as [[Construct]] caller
es5id: 15.3.4.3_A8_T6
description: >
    Checking if creating "new (Function("function
    f(){this.p1=1;};return f").apply())" fails
---*/

//CHECK#1
try {
  var obj = new (Function("function f(){this.p1=1;};return f").apply());
} catch (e) {
  $ERROR('#1: Function.prototype.apply can\'t be used as [[Construct]] caller');
}

//CHECK#2
if (obj.p1!== 1) {
  $ERROR('#2: Function.prototype.apply can\'t be used as [[Construct]] caller');
}
