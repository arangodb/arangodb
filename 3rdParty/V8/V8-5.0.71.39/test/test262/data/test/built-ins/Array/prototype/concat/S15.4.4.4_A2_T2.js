// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The concat function is intentionally generic.
    It does not require that its this value be an Array object
es5id: 15.4.4.4_A2_T2
description: Checking this for Object object with no items
---*/

var x = {};
x.concat = Array.prototype.concat;
var arr = x.concat();

//CHECK#0
arr.getClass = Object.prototype.toString;
if (arr.getClass() !== "[object " + "Array" + "]") {
  $ERROR('#0: var x = {}; x.concat = Array.prototype.concat; var arr = x.concat() arr is Array object. Actual: ' + (arr.getClass()));
}

//CHECK#1
if (arr[0] !== x) {
  $ERROR('#1: var x = {}; x.concat = Array.prototype.concat; var arr = x.concat(); arr[0] === x. Actual: ' + (arr[0]));
}

//CHECK#2
if (arr.length !== 1) {
  $ERROR('#2: var x = {}; x.concat = Array.prototype.concat; var arr = x.concat(); arr.length === 1. Actual: ' + (arr.length));
}
