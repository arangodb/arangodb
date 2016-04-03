// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.9-2-b-i-2
description: >
    Object.freeze - The [[Wrtiable]] attribute of all own data
    property of 'O' is set to false while other attributes are
    unchanged
includes: [propertyHelper.js]
---*/

var obj = {};

Object.defineProperty(obj, "foo1", {
    value: 10,
    writable: false,
    enumerable: true,
    configurable: false
});

Object.defineProperty(obj, "foo2", {
    value: 20,
    writable: true,
    enumerable: false,
    configurable: false
});

Object.freeze(obj);

var desc1 = Object.getOwnPropertyDescriptor(obj, "foo1");
var desc2 = Object.getOwnPropertyDescriptor(obj, "foo2");

verifyEqualTo(obj, "foo1", 10);

verifyNotWritable(obj, "foo1");

verifyEnumerable(obj, "foo1");

verifyNotConfigurable(obj, "foo1");

verifyEqualTo(obj, "foo2", 20);

verifyNotWritable(obj, "foo2");

verifyNotEnumerable(obj, "foo2");

verifyNotConfigurable(obj, "foo2");

if (desc1.configurable !== false) {
    $ERROR('Expected desc1.configurable === false, actually ' + desc1.configurable);
}

if (desc1.writable !== false) {
    $ERROR('Expected desc1.writable === false, actually ' + desc1.writable);
}

if (desc2.configurable !== false) {
    $ERROR('Expected desc2.configurable === false, actually ' + desc2.configurable);
}

if (desc2.writable !== false) {
    $ERROR('Expected desc2.writable === false, actually ' + desc2.writable);
}

