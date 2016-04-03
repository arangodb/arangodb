// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 13.1-13-s
description: >
    StrictMode - SyntaxError is thrown if 'arguments' occurs as the
    function name of a FunctionDeclaration in strict mode
flags: [onlyStrict]
---*/


assert.throws(SyntaxError, function() {
            eval("function arguments() { };")
});
