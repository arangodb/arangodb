// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.4.1-5-a-10-s
description: >
    Strict Mode - SyntaxError is thrown when deleting a variable of
    type Array
flags: [onlyStrict]
---*/

        var arrObj = [1,2,3];
assert.throws(SyntaxError, function() {
            eval("delete arrObj;");
});
