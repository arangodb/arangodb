// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.1.5_6-2-1-s
description: >
    Strict Mode - SyntaxError is thrown when an assignment to a
    reserved word or a future reserved word is contained in strict code
flags: [onlyStrict]
---*/


assert.throws(SyntaxError, function() {
            eval("var obj = {\
                get _11_1_5_6_2_1() {\
                   public = 42;\
                   return public;\
                }\
            };");

            var _11_1_5_6_2_1 = obj._11_1_5_6_2_1;
});
