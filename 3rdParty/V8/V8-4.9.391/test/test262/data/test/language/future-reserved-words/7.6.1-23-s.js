// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-23-s
description: >
    7.6 - SyntaxError expected: reserved words used as Identifier
    Names in UTF8: packag\u0065 (package)
flags: [onlyStrict]
---*/


assert.throws(SyntaxError, function() {
            eval("var packag\u0065 = 123;");
});
