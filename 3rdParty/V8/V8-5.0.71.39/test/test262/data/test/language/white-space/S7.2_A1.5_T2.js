// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: NO-BREAK SPACE (U+00A0) between any two tokens is allowed
es5id: 7.2_A1.5_T2
description: Insert real NO-BREAK SPACE between tokens of var x=1
---*/

var result;

//CHECK#1
eval("\u00A0var x\u00A0= 1\u00A0; result = x;");
if (result !== 1) {
  $ERROR('#1: eval("\\u00A0var x\\u00A0= 1\\u00A0; result = x;"); result === 1. Actual: ' + (result));
}

//CHECK#2
 var x = 2 ;
if (x !== 2) {
  $ERROR('#2:  var x = 1 ; x === 2. Actual: ' + (x));
}
