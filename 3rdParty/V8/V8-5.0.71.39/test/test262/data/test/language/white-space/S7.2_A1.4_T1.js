// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: SPACE (U+0020) between any two tokens is allowed
es5id: 7.2_A1.4_T1
description: Insert SPACE(\u0020) between tokens of var x=1
---*/

var result;

// CHECK#1
eval("\u0020var\u0020x\u0020=\u00201\u0020; result = x;");
if (result !== 1) {
  $ERROR('#1: eval("\\u0020var\\u0020x\\u0020=\\u00201\\u0020; result = x;"); result === 1;');
}

//CHECK#2
eval("\u0020" + "var" + "\u0020" + "x" + "\u0020" + "=" + "\u0020" + "2" + "\u0020; result = x;");
if (result !== 2) {
  $ERROR('#2: eval("\\u0020" + "var" + "\\u0020" + "x" + "\\u0020" + "=" + "\\u0020" + "2" + "\\u0020; result = x;"); result === 2. Actual: ' + (result));
}
