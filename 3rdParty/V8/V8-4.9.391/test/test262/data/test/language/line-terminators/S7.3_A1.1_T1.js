// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: LINE FEED (U+000A) may occur between any two tokens
es5id: 7.3_A1.1_T1
description: Insert LINE FEED (\u000A and \n) between tokens of var x=1
---*/

var result;

// CHECK#1
eval("\u000Avar\u000Ax\u000A=\u000A1\u000A; result = x;");
if (result !== 1) {
  $ERROR('#1: eval("\\u000Avar\\u000Ax\\u000A=\\u000A1\\u000A; result = x;"); result === 1. Actual: ' + (result));
}

//CHECK#2
eval("\u000A" + "var" + "\u000A" + "x" + "\u000A" + "=" + "\u000A" + "2" + "\u000A; result = x;");
if (result !== 2) {
  $ERROR('#2: eval("\\u000A" + "var" + "\\u000A" + "x" + "\\u000A" + "=" + "\\u000A" + "2" + "\\u000A; result = x;"); result === 2. Actual: ' + (result));
}

//CHECK#3
eval("\nvar\nx\n=\n3\n; result = x;");
if (result !== 3) {
  $ERROR('#3: eval("\\nvar\\nx\\n=\\n3\\n; result = x;"); result === 3. Actual: ' + (result));
}

//CHECK#4
eval("\n" + "var" + "\n" + "x" + "\n" + "=" + "\n" + "4" + "\n; result = x;");
if (result !== 4) {
  $ERROR('#4: eval("\\n" + "var" + "\\n" + "x" + "\\n" + "=" + "\\n" + "4" + "\\n; result = x;"); result === 4. Actual: ' + (result));
}

//CHECK#5
eval("\u000A" + "var" + "\n" + "x" + "\u000A" + "=" + "\n" + "5" + "\u000A; result = x;");
if (result !== 5) {
  $ERROR('#5: eval("\\u000A" + "var" + "\\n" + "x" + "\\u000A" + "=" + "\\n" + "5" + "\\u000A; result = x;"); result === 5. Actual: ' + (result));
}
