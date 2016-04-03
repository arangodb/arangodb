// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-228
description: >
    Object.defineProperties - 'O' is an Array, 'P' is an array index
    property, TypeError is thrown if the [[Configurable]] attribute
    value of 'P' is false, and [[Enumerable]] of 'desc' is present and
    its value is different from the [[Enumerable]] attribute value of
    'P'  (15.4.5.1 step 4.c)
includes: [propertyHelper.js]
---*/


var arr = [];

Object.defineProperty(arr, "1", {
    value: 3,
    configurable: false,
    enumerable: false

});

try {
    Object.defineProperties(arr, {
        "1": {
            value: 13,
            enumerable: true
        }
    });
    $ERROR("Expected an exception.");
} catch (e) {
    verifyEqualTo(arr, "1", 3);

    verifyNotWritable(arr, "1");

    verifyNotEnumerable(arr, "1");

    verifyNotConfigurable(arr, "1");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
