// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.4.4-2-2-s
description: Strict Mode - SyntaxError is thrown for ++arguments
flags: [onlyStrict]
---*/

function testcase() {
        var blah = arguments;
        assert.throws(SyntaxError, function() {
            eval("++arguments;");
        });
        assert.sameValue(blah, arguments, 'blah');
    }
testcase();
