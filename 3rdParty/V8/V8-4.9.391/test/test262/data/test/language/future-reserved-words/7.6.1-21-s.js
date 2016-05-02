// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-21-s
description: >
    7.6 - SyntaxError expected: reserved words used as Identifier
    Names in UTF8: \u0079ield (yield)
flags: [onlyStrict]
---*/


assert.throws(SyntaxError, function() {
            eval("var \u0079ield = 123;");
});
