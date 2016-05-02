// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Single line comments are terminated by the LINE FEED (U+000A) character
es5id: 7.3_A3.1_T1
description: Insert LINE FEED (\u000A) into single line comment
---*/

assert.throws(Test262Error, function() {
  eval("// single line \u000A throw new Test262Error();");
});
