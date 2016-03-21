// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: PARAGRAPH SEPARATOR (U+2029) may occur between any two tokens
es5id: 7.3_A1.4
description: Insert PARAGRAPH SEPARATOR (\u2029) between tokens of var x=1
---*/

var result;

// CHECK#1
eval("\u2029var\u2029x\u2029=\u20291\u2029; result = x;");
if (result !== 1) {
  $ERROR('#1: eval("\\u2029var\\u2029x\\u2029=\\u20291\\u2029"); result === 1. Actual: ' + (result));
}

//CHECK#2
eval("\u2029" + "var" + "\u2029" + "x" + "\u2029" + "=" + "\u2029" + "2" + "\u2029; result = x;");
if (result !== 2) {
  $ERROR('#2: eval("\\u2029" + "var" + "\\u2029" + "x" + "\\u2029" + "=" + "\\u2029" + "2" + "\\u2029"); result === 2. Actual: ' + (result));
}
