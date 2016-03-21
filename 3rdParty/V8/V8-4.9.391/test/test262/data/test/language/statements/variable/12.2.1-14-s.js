// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 12.2.1-14-s
description: >
    arguments - a function expr declaring a var named 'arguments'
    throws SyntaxError in strict mode
flags: [onlyStrict]
---*/


assert.throws(SyntaxError, function() {
    eval('(function (){var arguments;});');
});
