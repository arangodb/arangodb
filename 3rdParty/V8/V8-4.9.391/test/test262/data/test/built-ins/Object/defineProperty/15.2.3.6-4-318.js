// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-318
description: >
    Object.defineProperty - 'O' is an Arguments object, 'name' is
    generic own data property of 'O', test TypeError is thrown when
    updating the [[Writable]] attribute value of 'name' which is not
    configurable (10.6 [[DefineOwnProperty]] step 4)
includes: [propertyHelper.js]
---*/

(function () {
    Object.defineProperty(arguments, "genericProperty", {
        writable: false,
        configurable: false
    });
    try {
        Object.defineProperty(arguments, "genericProperty", {
            writable: true
        });
        $ERROR("Expected an exception.");
    } catch (e) {
        verifyEqualTo(arguments, "genericProperty", undefined);

        verifyNotWritable(arguments, "genericProperty");

        verifyNotEnumerable(arguments, "genericProperty");

        verifyNotConfigurable(arguments, "genericProperty");


        if (!(e instanceof TypeError)) {
            $ERROR("Expected TypeError, got " + e);
        }

    }

}(1, 2, 3));
