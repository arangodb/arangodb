// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-24-s
description: >
    7.6 - SyntaxError expected: reserved words used as Identifier
    Names in UTF8:
    \u0070\u0072\u006f\u0074\u0065\u0063\u0074\u0065\u0064 (protected)
flags: [onlyStrict]
---*/


assert.throws(SyntaxError, function() {
            eval("var \u0070\u0072\u006f\u0074\u0065\u0063\u0074\u0065\u0064 = 123;");
});
