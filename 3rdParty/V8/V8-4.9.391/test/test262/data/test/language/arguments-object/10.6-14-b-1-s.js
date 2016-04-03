// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 10.6-14-b-1-s
description: >
    Strict Mode - [[Enumerable]] attribute value in 'caller' is false
    under strict mode
flags: [onlyStrict]
---*/

        var argObj = function () {
            return arguments;
        } ();

        var verifyEnumerable = false;
        for (var _10_6_14_b_1 in argObj) {
            if (argObj.hasOwnProperty(_10_6_14_b_1) && _10_6_14_b_1 === "caller") {
                verifyEnumerable = true;
            }
        }

assert.sameValue(verifyEnumerable, false, 'verifyEnumerable');
assert(argObj.hasOwnProperty("caller"), 'argObj.hasOwnProperty("caller") !== true');
