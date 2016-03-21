// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.4.1-5-a-5-s
description: >
    Strict Mode - SyntaxError is thrown when deleting a variable which
    is a primitive type (boolean)
flags: [onlyStrict]
---*/

        var _11_4_1_5 = true;
assert.throws(SyntaxError, function() {
            eval("delete _11_4_1_5;");
});
