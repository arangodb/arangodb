// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: FORM FEED (U+000C) between any two tokens is allowed
es5id: 7.2_A1.3_T1
description: Insert FORM FEED(\u000C and \f) between tokens of var x=1
---*/

var result;

// CHECK#1
eval("\u000Cvar\u000Cx\u000C=\u000C1\u000C; result = x;");
if (result !== 1) {
  $ERROR('#1: eval("\\u000Cvar\\u000Cx\\u000C=\\u000C1\\u000C; result = x;"); result === 1. Actual: ' + (result));
}

//CHECK#2
eval("\u000C" + "var" + "\u000C" + "x" + "\u000C" + "=" + "\u000C" + "2" + "\u000C; result = x;");
if (result !== 2) {
  $ERROR('#2: eval("\\u000C" + "var" + "\\u000C" + "x" + "\\u000C" + "=" + "\\u000C" + "2" + "\\u000C; result = x;"); result === 2. Actual: ' + (result));
}

//CHECK#3
eval("\fvar\fx\f=\f3\f; result = x;");
if (result !== 3) {
  $ERROR('#3: eval("\\fvar\\fx\\f=\\f3\\f; result = x;"); result === 3. Actual: ' + (result));
}

//CHECK#4
eval("\f" + "var" + "\f" + "x" + "\f" + "=" + "\f" + "4" + "\f; result = x;");
if (result !== 4) {
  $ERROR('#4: eval("\\f" + "var" + "\\f" + "x" + "\\f" + "=" + "\\f" + "4" + "\\f; result = x;"); result === 4. Actual: ' + (result));
}

//CHECK#5
eval("\u000C" + "var" + "\f" + "x" + "\u000C" + "=" + "\f" + "5" + "\u000C; result = x;");
if (result !== 5) {
  $ERROR('#5: eval("\\u000C" + "var" + "\\f" + "x" + "\\u000C" + "=" + "\\f" + "5" + "\\u000C; result = x;"); result === 5. Actual: ' + (result));
}
