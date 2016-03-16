// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "\"This\" operator only evaluates Expression"
es5id: 11.1.6_A3_T5
description: Using grouping operator in declaration of variables
---*/

//CHECK#1
var x;
(x) = 1;
if (x !== 1) {
  $ERROR('#1: (x) = 1; x === 1. Actual: ' + (x));
}

//CHECK#2
var y = 1; (y)++; ++(y); (y)--; --(y);
if (y !== 1) {
  $ERROR('#2: var y = 1; (y)++; ++(y); (y)--; --(y); y === 1. Actual: ' + (y));
}
