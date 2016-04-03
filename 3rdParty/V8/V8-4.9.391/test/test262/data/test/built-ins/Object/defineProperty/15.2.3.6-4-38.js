// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-38
description: >
    Object.defineProperty - 'O' is the Math object that uses Object's
    [[GetOwnProperty]] method to access the 'name' property (8.12.9
    step 1)
includes: [propertyHelper.js]
---*/

try {
    Object.defineProperty(Math, "foo", {
        value: 12,
        configurable: true
    });
    
    verifyEqualTo(Math, "foo", 12);

    verifyNotWritable(Math, "foo");

    verifyNotEnumerable(Math, "foo");

    verifyConfigurable(Math, "foo");
} finally {
    delete Math.foo;
}

