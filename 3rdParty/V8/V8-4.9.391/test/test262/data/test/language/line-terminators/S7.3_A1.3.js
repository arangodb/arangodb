// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: LINE SEPARATOR (U+2028) may occur between any two tokens
es5id: 7.3_A1.3
description: Insert LINE SEPARATOR (\u2028) between tokens of var x=1
---*/

var result;

// CHECK#1
eval("\u2028var\u2028x\u2028=\u20281\u2028; result = x;");
if (result !== 1) {
  $ERROR('#1: eval("\\u2028var\\u2028x\\u2028=\\u20281\\u2028"); result === 1. Actual: ' + (result));
}

//CHECK#2
eval("\u2028" + "var" + "\u2028" + "x" + "\u2028" + "=" + "\u2028" + "2" + "\u2028; result = x;");
if (result !== 2) {
  $ERROR('#2: eval("\\u2028" + "var" + "\\u2028" + "x" + "\\u2028" + "=" + "\\u2028" + "2" + "\\u2028"); result === 2. Actual: ' + (result));
}
