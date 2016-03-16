// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-293-4
description: >
    Object.defineProperty - 'O' is an Arguments object of a function
    that has formal parameters, 'name' is own data property of 'O'
    which is also defined in [[ParameterMap]] of 'O', test TypeError
    is not thrown when updating the [[Value]] attribute value of
    'name' which is defined as non-writable and configurable (10.6
    [[DefineOwnProperty]] step 3 and step 5.b)
includes: [propertyHelper.js]
flags: [onlyStrict]
---*/

(function (a, b, c) {
    Object.defineProperty(arguments, "0", {
        value: 10,
        writable: false,
    });
    Object.defineProperty(arguments, "0", {
        value: 20
    });
    if (a !== 0) {
        $ERROR('Expected "a === 0", actually ' + a);
    }

    verifyEqualTo(arguments, "0", 20);

    verifyNotWritable(arguments, "0");

    verifyEnumerable(arguments, "0");

    verifyConfigurable(arguments, "0");
}(0, 1, 2));
