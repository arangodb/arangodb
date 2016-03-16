// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 10.1.1-15-s
description: >
    Strict Mode - Function code that is part of a FunctionDeclaration
    is strict function code if FunctionDeclaration is contained in use
    strict
flags: [noStrict]
---*/

function testcase() {
        "use strict";
        function fun() {
            eval("var public = 1;");
        }

        assert.throws(SyntaxError, function() {
            fun();
        });
    }
testcase();
