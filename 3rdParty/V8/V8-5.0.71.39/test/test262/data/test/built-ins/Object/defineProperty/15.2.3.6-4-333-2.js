// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-333-2
description: >
    Object.defineProperty will update [[Value]] attribute of indexed
    property 'P' successfully when [[Configurable]] attribute is
    false, [[Writable]] attribute is true and 'A' is an Array object
    (8.12.9 - step 10)
includes: [propertyHelper.js]
---*/


var obj = [];

Object.defineProperty(obj, "0", {
    value: 1001,
    writable: true,
    configurable: false
});

Object.defineProperty(obj, "0", {
    value: 1002
});

verifyEqualTo(obj, "0", 1002);

verifyWritable(obj, "0");

verifyNotEnumerable(obj, "0");

verifyNotConfigurable(obj, "0");

