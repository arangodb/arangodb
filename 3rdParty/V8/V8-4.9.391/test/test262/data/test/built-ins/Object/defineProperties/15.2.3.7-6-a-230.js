// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-230
description: >
    Object.defineProperties - 'O' is an Array, 'P' is an array index
    property, TypeError is thrown if  'P' is data property, and'desc'
    is accessor descriptor, and the [[Configurable]] attribute value
    of 'P' is false  (15.4.5.1 step 4.c)
includes: [propertyHelper.js]
---*/


var arr = [];

Object.defineProperty(arr, "1", {
    value: 3,
    configurable: false
});

try {
    Object.defineProperties(arr, {
        "1": {
            set: function () { }
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
