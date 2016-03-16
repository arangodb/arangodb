// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 10.1.1-23-s
description: >
    Strict Mode - Function code of a FunctionExpression contains Use
    Strict Directive which appears in the middle of the block
flags: [noStrict]
---*/

        (function () {
            eval("var public = 1;");
            assert.sameValue(public, 1);
            "use strict";
        }) ();
