// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: VERTICAL TAB (U+000B) between any two tokens is allowed
es5id: 7.2_A1.2_T1
description: Insert VERTICAL TAB(\u000B and \v) between tokens of var x=1
---*/

var result;

// CHECK#1
eval("\u000Bvar\u000Bx\u000B=\u000B1\u000B; result = x;");
if (result !== 1) {
  $ERROR('#1: eval("\\u000Bvar\\u000Bx\\u000B=\\u000B1\\u000B; result = x;"); result === 1. Actual: ' + (result));
}

//CHECK#2
eval("\u000B" + "var" + "\u000B" + "x" + "\u000B" + "=" + "\u000B" + "2" + "\u000B; result = x;");
if (result !== 2) {
  $ERROR('#2: eval("\\u000B" + "var" + "\\u000B" + "x" + "\\u000B" + "=" + "\\u000B" + "2" + "\\u000B; result = x;"); result === 2. Actual: ' + (result));
}

//CHECK#3
eval("\vvar\vx\v=\v3\v; result = x;");
if (result !== 3) {
  $ERROR('#3: eval("\\vvar\\vx\\v=\\v3\\v; result = x;"); x === 3. Actual: ' + (result));
}

//CHECK#4
eval("\v" + "var" + "\v" + "x" + "\v" + "=" + "\v" + "4" + "\v; result = x;");
if (result !== 4) {
  $ERROR('#4: eval("\\v" + "var" + "\\v" + "x" + "\\v" + "=" + "\\v" + "4" + "\\v; result = x;"); result === 4. Actual: ' + (result));
}

//CHECK#5
eval("\u000B" + "var" + "\v" + "x" + "\u000B" + "=" + "\v" + "5" + "\u000B; result = x;");
if (result !== 5) {
  $ERROR('#5: eval("\\u000B" + "var" + "\\v" + "x" + "\\u000B" + "=" + "\\v" + "5" + "\\u000B; result = x;"); result === 5. Actual: ' + (result));
}
