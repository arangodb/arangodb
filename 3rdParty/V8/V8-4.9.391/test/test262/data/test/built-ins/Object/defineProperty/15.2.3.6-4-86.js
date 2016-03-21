// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-86
description: >
    Object.defineProperty will throw TypeError when name.configurable
    = false, name.writable = false, desc.value = +0 and name.value =
    -0 (8.12.9 step 10.a.ii.1)
includes: [propertyHelper.js]
---*/


var obj = {};

Object.defineProperty(obj, "foo", { 
    value: -0, 
    writable: false, 
    configurable: false 
});

try {
    Object.defineProperty(obj, "foo", { value: +0 });
    $ERROR("Expected an exception.");
} catch (e) {
    verifyEqualTo(obj, "foo", -0);

    verifyNotWritable(obj, "foo");

    verifyNotEnumerable(obj, "foo");

    verifyNotConfigurable(obj, "foo");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
