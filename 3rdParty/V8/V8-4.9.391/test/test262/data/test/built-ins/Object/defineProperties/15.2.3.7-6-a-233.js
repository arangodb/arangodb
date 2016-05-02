// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-233
description: >
    Object.defineProperties - 'O' is an Array, 'P' is an array index
    property, 'P' is data property and 'desc' is data descriptor, and
    the [[Configurable]] attribute value of 'P' is false, test
    TypeError is thrown if the [[Writable]] attribute value of 'P' is
    false and the [[Writable]] field of 'desc' is true.  (15.4.5.1
    step 4.c)
includes: [propertyHelper.js]
---*/


var arr = [];

Object.defineProperty(arr, "1", {
    configurable: false,
    writable: false

});

try {
    Object.defineProperties(arr, {
        "1": {
            writable: true
        }
    });
    $ERROR("Expected an exception.");
} catch (e) {
    verifyEqualTo(arr, "1", undefined);

    verifyNotWritable(arr, "1");

    verifyNotEnumerable(arr, "1");

    verifyNotConfigurable(arr, "1");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
