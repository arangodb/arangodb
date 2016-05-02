// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 12.2.1-32-s
description: >
    arguments as local var identifier defined twice and assigned once
    throws SyntaxError in strict mode
flags: [onlyStrict]
---*/


assert.throws(SyntaxError, function() {
    eval('function foo() { var arguments, arguments = 42;}');
});
