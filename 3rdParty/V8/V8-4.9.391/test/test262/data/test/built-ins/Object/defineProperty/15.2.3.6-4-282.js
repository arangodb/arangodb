// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-282
description: >
    Object.defineProperty - 'O' is an Array, 'name' is generic own
    data property of 'O', test TypeError is thrown when updating the
    [[Writable]] attribute value of 'name' which is defined as
    non-configurable (15.4.5.1 step 5)
includes: [propertyHelper.js]
---*/


var arrObj = [];

Object.defineProperty(arrObj, "property", {
    writable: false
});
try {
    Object.defineProperty(arrObj, "property", {
        writable: true
    });
    $ERROR("Expected an exception.");
} catch (e) {
    verifyEqualTo(arrObj, "property", undefined);

    verifyNotWritable(arrObj, "property");

    verifyNotEnumerable(arrObj, "property");

    verifyNotConfigurable(arrObj, "property");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
