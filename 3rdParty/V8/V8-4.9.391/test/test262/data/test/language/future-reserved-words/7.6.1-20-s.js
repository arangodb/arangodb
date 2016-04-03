// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-20-s
description: >
    7.6 - SyntaxError expected: reserved words used as Identifier
    Names in UTF8: \u0070\u0075\u0062\u006c\u0069\u0063 (public)
flags: [onlyStrict]
---*/


assert.throws(SyntaxError, function() {
            eval("var \u0070\u0075\u0062\u006c\u0069\u0063 = 123;");
});
