// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: HORIZONTAL TAB (U+0009) between any two tokens is allowed
es5id: 7.2_A1.1_T1
description: Insert HORIZONTAL TAB(\u0009 and \t) between tokens of var x=1
---*/

var result;

// CHECK#1
eval("\u0009var\u0009x\u0009=\u00091\u0009; result = x;");
if (result !== 1) {
  $ERROR('#1: eval("\\u0009var\\u0009x\\u0009=\\u00091\\u0009; result = x;"); result === 1. Actual: ' + (result));
}

//CHECK#2
eval("\u0009" + "var" + "\u0009" + "x" + "\u0009" + "=" + "\u0009" + "2" + "\u0009; result = x;");
if (result !== 2) {
  $ERROR('#2: eval("\\u0009" + "var" + "\\u0009" + "x" + "\\u0009" + "=" + "\\u0009" + "2" + "\\u0009; result = x;"); result === 2. Actual: ' + (result));
}

//CHECK#3
eval("\tvar\tx\t=\t3\t; result = x;");
if (result !== 3) {
  $ERROR('#3: eval("\\tvar\\tx\\t=\\t3\\t; result = x;"); x === 3. Actual: ' + (result));
}

//CHECK#4
eval("\t" + "var" + "\t" + "x" + "\t" + "=" + "\t" + "4" + "\t; result = x;");
if (result !== 4) {
  $ERROR('#4: eval("\\t" + "var" + "\\t" + "x" + "\\t" + "=" + "\\t" + "4" + "\\t; result = x;"); result === 4. Actual: ' + (result));
}

//CHECK#5
eval("\u0009" + "var" + "\t" + "x" + "\u0009" + "=" + "\t" + "5" + "\u0009; result = x;");
if (result !== 5) {
  $ERROR('#5: eval("\\u0009" + "var" + "\\t" + "x" + "\\u0009" + "=" + "\\t" + "5" + "\\u0009; result = x;"); result === 5. Actual: ' + (result));
}
