// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-317-1
description: >
    Object.defineProperty - 'O' is an Arguments object of a function
    that has formal parameters, 'P' is own data property of 'O', test
    TypeError is thrown when updating the [[Value]] attribute value of
    'P' which is not writable and not configurable (10.6
    [[DefineOwnProperty]] step 4)
includes: [propertyHelper.js]
---*/

(function (a, b, c) {
    Object.defineProperty(arguments, "genericProperty", {
        value: 1001,
        writable: false,
        configurable: false
    });
    try {
        Object.defineProperty(arguments, "genericProperty", {
            value: 1002
        });
        $ERROR("Expected an exception.");
    } catch (e) {
        if (b !== 2) {
            $ERROR('Expected "b === 2;", actually ' + b);
        }

        verifyEqualTo(arguments, "genericProperty", 1001);

        verifyNotWritable(arguments, "genericProperty");

        verifyNotEnumerable(arguments, "genericProperty");

        verifyNotConfigurable(arguments, "genericProperty");

        if (!(e instanceof TypeError)) {
            $ERROR("Expected TypeError, got " + e);
        }

    }
}(1, 2, 3));
