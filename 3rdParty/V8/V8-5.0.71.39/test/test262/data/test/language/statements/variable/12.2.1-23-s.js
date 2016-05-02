// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 12.2.1-23-s
description: >
    arguments as local var identifier assigned to throws SyntaxError
    in strict mode
flags: [onlyStrict]
---*/


assert.throws(SyntaxError, function() {
    eval('function foo() { var arguments = 42;}');
});
