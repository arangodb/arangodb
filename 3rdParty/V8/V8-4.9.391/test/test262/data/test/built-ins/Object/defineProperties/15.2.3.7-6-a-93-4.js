// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-93-4
description: >
    Object.defineProperties will fail to update [[Value]] attribute of
    indexed data property 'P' when [[Configurable]] attribute of first
    updating property are false  (8.12.9 - step Note & 10.a.ii.1)
includes: [propertyHelper.js]
---*/


var obj = {};

Object.defineProperty(obj, "0", {
    value: 1001,
    writable: false,
    configurable: false
});

Object.defineProperty(obj, "1", {
    value: 1003,
    writable: false,
    configurable: true
});

try {
    Object.defineProperties(obj, {
        0: {
            value: 1002
        },
        1: {
            value: 1004
        }
    });

    $ERROR("Expected an exception.");
} catch (e) {
    verifyEqualTo(obj, "0", 1001);

    verifyNotWritable(obj, "0");

    verifyNotEnumerable(obj, "0");

    verifyNotConfigurable(obj, "0");
    verifyEqualTo(obj, "1", 1003);

    verifyNotWritable(obj, "1");

    verifyNotEnumerable(obj, "1");

    verifyConfigurable(obj, "1");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
