// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-311
description: >
    Object.defineProperties - 'O' is an Arguments object, 'P' is
    generic own accessor property of 'O', test TypeError is thrown
    when updating the [[Set]] attribute value of 'P' which is not
    configurable (10.6 [[DefineOwnProperty]] step 4)
includes: [propertyHelper.js]
---*/

var arg = (function () {
    return arguments;
}(1, 2, 3));

function setFun(value) {
    arg.genericPropertyString = value;
}
Object.defineProperty(arg, "genericProperty", {
    set: setFun,
    configurable: false
});

try {
    Object.defineProperties(arg, {
        "genericProperty": {
            set: function (value) {
                arg.genericPropertyString1 = value;
            }
        }
    });

    $ERROR("Expected an exception.");
} catch (e) {
    verifyWritable(arg, "genericProperty", "genericPropertyString");

    verifyNotEnumerable(arg, "genericProperty");

    verifyNotConfigurable(arg, "genericProperty");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
