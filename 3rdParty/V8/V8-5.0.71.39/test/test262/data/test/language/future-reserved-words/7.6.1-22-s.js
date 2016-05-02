// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-22-s
description: >
    7.6 - SyntaxError expected: reserved words used as Identifier
    Names in UTF8: inte\u0072face (interface)
flags: [onlyStrict]
---*/


assert.throws(SyntaxError, function() {
            eval("var inte\u0072face = 123;");
});
