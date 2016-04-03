// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.4.1-3-a-1-s
description: >
    Strict Mode - SyntaxError is thrown when deleting an un-resolvable
    reference
flags: [onlyStrict]
---*/


assert.throws(SyntaxError, function() {
            eval("delete obj");
});
