// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 12.10.1-16-s
description: >
    Strict Mode - SyntaxError is thrown when the RHS of an object
    indexer assignment utilizes WithStatement
flags: [onlyStrict]
---*/


assert.throws(SyntaxError, function() {
            eval("var obj = {}; obj['get'] = function (a) { with(a){} };  ");
});
