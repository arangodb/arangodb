// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-13
description: >
    Object.defineProperties - 'O' is an Array object which implements
    its own [[GetOwnProperty]] method to get 'P' (8.12.9 step 1 )
includes: [propertyHelper.js]
---*/

var arr = [];

Object.defineProperty(arr, "prop", {
    value: 11,
    configurable: false
});

try {
    Object.defineProperties(arr, {
        prop: {
            value: 12,
            configurable: true
        }
    });
    $ERROR("Expected an exception.");
} catch (e) {
    verifyEqualTo(arr, "prop", 11);

    verifyNotWritable(arr, "prop");

    verifyNotEnumerable(arr, "prop");

    verifyNotConfigurable(arr, "prop");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
