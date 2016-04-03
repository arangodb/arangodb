// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.4.5-2-1-s
description: Strict Mode - SyntaxError is thrown for --eval
flags: [onlyStrict]
---*/

        var blah = eval;
assert.throws(SyntaxError, function() {
            eval("--eval;");
});
assert.sameValue(blah, eval, 'blah');
