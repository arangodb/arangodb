// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.9-2-c-4
description: >
    Object.freeze - all own properties of 'O' are not writable and not
    configurable
includes: [propertyHelper.js]
---*/

var obj = {};
var resultSetFun = false;

Object.defineProperty(obj, "foo1", {
    value: 10,
    writable: false,
    enumerable: true,
    configurable: false
});

function get_func() {
    return 10;
}

function set_func() {
    resultSetFun = true;
}

Object.defineProperty(obj, "foo2", {
    get: get_func,
    set: set_func,
    enumerable: true,
    configurable: true
});

Object.freeze(obj);

verifyEqualTo(obj, "foo2", 10);

verifyNotConfigurable(obj, "foo2");

obj.foo2 = 12;
if (!resultSetFun) {
    $ERROR('Expected obj["foo2"] set() to be called, but was not.');
}

verifyEnumerable(obj, "foo2");

var desc1 = Object.getOwnPropertyDescriptor(obj, "foo1");
if (desc1.configurable || desc1.writable) {
    $ERROR('Expected obj["foo1"] to be non-writable, non-configurable; actually ' + JSON.stringify(desc1));
}

var desc2 = Object.getOwnPropertyDescriptor(obj, "foo2");
if (desc2.configurable || desc2.writable) {
    $ERROR('Expected obj["foo2"] to be non-writable, non-configurable; actually ' + JSON.stringify(desc2));
}

verifyEqualTo(obj, "foo1", 10);

verifyNotWritable(obj, "foo1");

verifyEnumerable(obj, "foo1");

verifyNotConfigurable(obj, "foo1");
