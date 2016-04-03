// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.8.5-1
description: >
    Literal RegExp Objects - SyntaxError exception is thrown if the
    RegularExpressionNonTerminator position of a
    RegularExpressionBackslashSequence is a LineTerminator.
---*/


assert.throws(SyntaxError, function() {
            eval("var regExp = /\\\rn/;");
});
