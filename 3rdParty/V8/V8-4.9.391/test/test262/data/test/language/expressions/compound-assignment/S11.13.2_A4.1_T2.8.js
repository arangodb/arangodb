// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The production x *= y is the same as the production x = x * y
es5id: 11.13.2_A4.1_T2.8
description: >
    Type(x) is different from Type(y) and both types vary between
    Boolean (primitive or object) and Undefined
---*/

var x;

//CHECK#1
x = true;
x *= undefined;
if (isNaN(x) !== true) {
  $ERROR('#1: x = true; x *= undefined; x === Not-a-Number. Actual: ' + (x));
}

//CHECK#2
x = undefined;
x *= true;
if (isNaN(x) !== true) {
  $ERROR('#2: x = undefined; x *= true; x === Not-a-Number. Actual: ' + (x));
}

//CHECK#3
x = new Boolean(true);
x *= undefined;
if (isNaN(x) !== true) {
  $ERROR('#3: x = new Boolean(true); x *= undefined; x === Not-a-Number. Actual: ' + (x));
}

//CHECK#4
x = undefined;
x *= new Boolean(true);
if (isNaN(x) !== true) {
  $ERROR('#4: x = undefined; x *= new Boolean(true); x === Not-a-Number. Actual: ' + (x));
}
