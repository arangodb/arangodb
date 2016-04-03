// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: SPACE (U+0020) between any two tokens is allowed
es5id: 7.2_A1.4_T2
description: Insert real SPACE between tokens of var x=1
---*/

var result;

//CHECK#1
eval("\u0020var x\u0020= 1\u0020; result = x;");
if (result !== 1) {
  $ERROR('#1: eval("\\u0020var x\\u0020= 1\\u0020; result = x;"); result === 1. Actual: ' + (result));
}

//CHECK#2
 var x = 2 ;
if (x !== 2) {
  $ERROR('#2:  var x = 2 ; x === 2. Actual: ' + (x));
}
