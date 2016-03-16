// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-306
description: >
    Object.defineProperties - 'O' is an Arguments object, 'P' is
    generic own data property of 'O', test TypeError is thrown when
    updating the [[Value]] attribute value of 'P' which is not
    writable and not configurable (10.6 [[DefineOwnProperty]] step 4)
includes: [propertyHelper.js]
---*/

var arg = (function () {
    return arguments;
}(1, 2, 3));

Object.defineProperty(arg, "genericProperty", {
    value: 1001,
    writable: false,
    configurable: false
});

try {
    Object.defineProperties(arg, {
        "genericProperty": {
            value: 1002
        }
    });

    $ERROR("Expected an exception.");
} catch (e) {
    verifyEqualTo(arg, "genericProperty", 1001);

    verifyNotWritable(arg, "genericProperty");

    verifyNotEnumerable(arg, "genericProperty");

    verifyNotConfigurable(arg, "genericProperty");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
