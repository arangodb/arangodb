// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-18-s
description: >
    7.6 - SyntaxError expected: reserved words used as Identifier
    Names in UTF8: l\u0065t (let)
flags: [onlyStrict]
---*/

        
assert.throws(SyntaxError, function() {
            eval("var l\u0065t = 123;");
});
