// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-354-10
description: >
    Object.defineProperty will update [[Value]] attribute of indexed
    property 'P' successfully when [[Configurable]] attribute is true
    and [[Writable]] attribute is false, 'O' is an Object object
    (8.12.9 step - Note)
includes: [propertyHelper.js]
---*/


var obj = {};

Object.defineProperty(obj, "0", {
    value: 1001,
    writable: false,
    configurable: true
});

Object.defineProperty(obj, "0", {
    value: 1002
});

verifyEqualTo(obj, "0", 1002);

verifyNotWritable(obj, "0");

verifyNotEnumerable(obj, "0");

verifyConfigurable(obj, "0");

