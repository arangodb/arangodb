// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-274
description: >
    Object.defineProperties - 'O' is an Array, 'P' is generic own
    accessor property of 'O', test TypeError is thrown when updating
    the [[Get]] attribute value of 'P' which is defined as
    non-configurable (15.4.5.1 step 5)
includes: [propertyHelper.js]
---*/

var arr = [];

function get_fun() {
    return 37;
}
function set_fun(value) {
    arr.verifySetFun = value;
}
Object.defineProperty(arr, "property", {
    get: get_fun,
    set: set_fun
});

try {
    Object.defineProperties(arr, {
        "property": {
            get: function () {
                return 36;
            }
        }
    });
    $ERROR("Expected an exception.");
} catch (e) {
    verifyEqualTo(arr, "property", get_fun());

    verifyWritable(arr, "property", "verifySetFun");

    verifyNotEnumerable(arr, "property");

    verifyNotConfigurable(arr, "property");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
