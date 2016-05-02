// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The initial value of Number.prototype.constructor is the
    built-in Number constructor
es5id: 15.7.4.1_A1
description: Compare Number.prototype.constructor with Number
---*/

//CHECK#1
if(Number.prototype.constructor !== Number){
  $ERROR('#1: Number.prototype.constructor === Number');
}
