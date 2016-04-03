// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 12.10.1-14-s
description: >
    Strict Mode - SyntaxError is thrown when the getter of a literal
    object utilizes WithStatement
flags: [onlyStrict]
---*/


assert.throws(SyntaxError, function() {
            eval("var obj = { get(a) { with(a){} } }; ");
});
