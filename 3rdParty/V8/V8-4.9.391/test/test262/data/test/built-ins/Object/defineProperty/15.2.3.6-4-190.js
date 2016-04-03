// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-190
description: >
    Object.defineProperty - 'O' is an Array, 'name' is an array index
    named property, 'name' is own data property, test TypeError is
    thrown on updating the configurable attribute from false to true
    (15.4.5.1 step 4.c)
includes: [propertyHelper.js]
---*/

var arrObj = [];

Object.defineProperty(arrObj, 0, {
    value: "ownDataProperty",
    configurable: false
});

try {
    Object.defineProperty(arrObj, 0, {
        configurable: true
    });
        $ERROR("Expected an exception.");
} catch (e) {
    verifyEqualTo(arrObj, "0", "ownDataProperty");

    verifyNotWritable(arrObj, "0");

    verifyNotEnumerable(arrObj, "0");

    verifyNotConfigurable(arrObj, "0");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }
}
