// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 12.2.1-15-s
description: >
    arguments - a function expr assigning into 'arguments' throws a
    SyntaxError in strict mode
flags: [onlyStrict]
---*/


assert.throws(SyntaxError, function() {
    eval('(function () {arguments = 42;})()');
});
