// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-276
description: >
    Object.defineProperties - 'O' is an Array, 'P' is generic own
    accessor property of 'O', test TypeError is thrown when updating
    the [[Enumerable]] attribute value of 'P' which is defined as
    non-configurable (15.4.5.1 step 5)
includes: [propertyHelper.js]
---*/


var arr = [];

function set_fun(value) {
    arr.setVerifyHelpProp = value;
}
Object.defineProperty(arr, "property", {
    set: set_fun,
    enumerable: false
});

try {
    Object.defineProperties(arr, {
        "property": {
            enumerable: true
        }
    });
    $ERROR("Expected an exception.");
} catch (e) {
    verifyWritable(arr, "property", "setVerifyHelpProp");

    verifyNotEnumerable(arr, "property");

    verifyNotConfigurable(arr, "property");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
