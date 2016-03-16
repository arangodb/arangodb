// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.13.1-4-31-s
description: >
    Strict Mode - SyntaxError is thrown if the identifier 'arguments'
    appears as the LeftHandSideExpression (PrimaryExpression) of
    simple assignment(=) under strict mode
flags: [onlyStrict]
---*/

function testcase() {
        var err = null;
        var blah = arguments;
        try {
            eval("(arguments) = 20;");
        } catch (e) {
            err = e;
        }
        assert(err instanceof SyntaxError, 'err instanceof SyntaxError');
        assert.sameValue(blah, arguments, 'blah');
}
testcase();
