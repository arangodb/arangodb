// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-12
description: >
    Object.defineProperties - 'O' is a Function object which
    implements its own [[GetOwnProperty]] method to get 'P' (8.12.9
    step 1 )
includes: [propertyHelper.js]
---*/

var fun = function () { };

Object.defineProperty(fun, "prop", {
    value: 11,
    configurable: false
});

try {
    Object.defineProperties(fun, {
        prop: {
            value: 12,
            configurable: true
        }
    });
    $ERROR("Expected an exception.");
} catch (e) {
    verifyEqualTo(fun, "prop", 11);

    verifyNotWritable(fun, "prop");

    verifyNotEnumerable(fun, "prop");

    verifyNotConfigurable(fun, "prop");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
