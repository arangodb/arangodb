// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-66
description: >
    Object.defineProperties throws TypeError when P.configurable is
    false, P.enumerable and desc.enumerable has different values
    (8.12.9 step 7.b)
includes: [propertyHelper.js]
---*/


var obj = {};

Object.defineProperty(obj, "foo", { 
    value: 10, 
    enumerable: true, 
    configurable: false 
});

try {
    Object.defineProperties(obj, {
        foo: {
            enumerable: false
        }
    });
    $ERROR("Expected an exception.");
} catch (e) {
    verifyEqualTo(obj, "foo", 10);

    verifyNotWritable(obj, "foo");

    verifyEnumerable(obj, "foo");

    verifyNotConfigurable(obj, "foo");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
