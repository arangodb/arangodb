// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: NO-BREAK SPACE (U+00A0) between any two tokens is allowed
es5id: 7.2_A1.5_T1
description: Insert NO-BREAK SPACE(\u00A0) between tokens of var x=1
---*/

var result;

// CHECK#1
eval("\u00A0var\u00A0x\u00A0=\u00A01\u00A0; result = x;");
if (result !== 1) {
  $ERROR('#1: eval("\\u00A0var\\u00A0x\\u00A0=\\u00A01\\u00A0; result = x;"); result === 1. Actual: ' + (result));
}

//CHECK#2
eval("\u00A0" + "var" + "\u00A0" + "x" + "\u00A0" + "=" + "\u00A0" + "2" + "\u00A0; result = x;");
if (result !== 2) {
  $ERROR('#2: eval("\\u00A0" + "var" + "\\u00A0" + "x" + "\\u00A0" + "=" + "\\u00A0" + "2" + "\\u00A0; result = x;"); result === 2. Actual: ' + (result));
}
