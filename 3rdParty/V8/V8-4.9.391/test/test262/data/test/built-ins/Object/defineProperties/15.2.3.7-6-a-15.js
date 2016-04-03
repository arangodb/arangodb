// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-15
description: >
    Object.defineProperties - 'O' is a Boolean object which implements
    its own [[GetOwnProperty]] method to get 'P' (8.12.9 step 1 )
includes: [propertyHelper.js]
---*/


var obj = new Boolean(false);

Object.defineProperty(obj, "prop", {
    value: 11,
    configurable: false
});

try {
    Object.defineProperties(obj, {
        prop: {
            value: 12,
            configurable: true
        }
    });
    $ERROR("Expected an exception.");
} catch (e) {
    verifyEqualTo(obj, "prop", 11);

    verifyNotWritable(obj, "prop");

    verifyNotEnumerable(obj, "prop");

    verifyNotConfigurable(obj, "prop");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
