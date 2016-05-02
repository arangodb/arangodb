// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 12.10.1-2-s
description: >
    with statement in strict mode throws SyntaxError (nested function
    where container is strict)
flags: [noStrict]
---*/


assert.throws(SyntaxError, function() {
    // wrapping it in eval since this needs to be a syntax error. The
    // exception thrown must be a SyntaxError exception.
    eval("\
          function foo() {\
            \'use strict\'; \
            function f() {\
                var o = {}; \
                with (o) {};\
            }\
          }\
        ");
});
