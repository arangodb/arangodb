// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-91
description: >
    Object.defineProperty will throw TypeError when name.configurable
    = false, name.writable = false, desc.value and name.value are two
    strings with different values (8.12.9 step 10.a.ii.1)
includes: [propertyHelper.js]
---*/


var obj = {};

Object.defineProperty(obj, "foo", {
    value: "abcd",
    writable: false,
    configurable: false 
});

try {
    Object.defineProperty(obj, "foo", { value: "fghj" });
    $ERROR("Expected an exception.");
} catch (e) {
    verifyEqualTo(obj, "foo", "abcd");

    verifyNotWritable(obj, "foo");

    verifyNotEnumerable(obj, "foo");

    verifyNotConfigurable(obj, "foo");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
