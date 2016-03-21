// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-68
description: >
    Object.defineProperties throws TypeError when P is data property
    and  P.configurable is false, desc is accessor property (8.12.9
    step 9.a)
includes: [propertyHelper.js]
---*/


var obj = {};

Object.defineProperty(obj, "foo", {
    value: 10,
    configurable: false
});

function get_func() {
    return 11;
}

try {
    Object.defineProperties(obj, {
        foo: {
            get: get_func
        }
    });
    $ERROR("Expected an exception.");
} catch (e) {
    verifyEqualTo(obj, "foo", 10);

    verifyNotWritable(obj, "foo");

    verifyNotEnumerable(obj, "foo");

    verifyNotConfigurable(obj, "foo");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
