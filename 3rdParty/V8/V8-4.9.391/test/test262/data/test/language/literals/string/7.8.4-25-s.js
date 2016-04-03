// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.8.4-25-s
description: An OctalEscapeSequence is not allowed in a String under Strict Mode
flags: [onlyStrict]
---*/


assert.throws(SyntaxError, function() {
    eval('var x = "\\106";');
});
