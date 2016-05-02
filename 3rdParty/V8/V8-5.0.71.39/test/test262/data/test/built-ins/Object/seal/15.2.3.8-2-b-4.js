// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.8-2-b-4
description: >
    Object.seal - all own properties of 'O' are already
    non-configurable
includes: [propertyHelper.js]
---*/

var obj = {};
obj.variableForHelpVerify = "data";

Object.defineProperty(obj, "foo1", {
    value: 10,
    writable: true,
    enumerable: true,
    configurable: false
});

function set_func(value) {
    obj.variableForHelpVerify = value;
}
function get_func() {
    return 10;
}
Object.defineProperty(obj, "foo2", {
    get: get_func,
    set: set_func,
    enumerable: true,
    configurable: false
});

if (!Object.isExtensible(obj)) {
    $ERROR('Expected obj to be extensible, actually ' + Object.isExtensible(obj));
}

Object.seal(obj);

if (Object.isExtensible(obj)) {
    $ERROR('Expected obj NOT to be extensible, actually ' + Object.isExtensible(obj));
}

verifyEqualTo(obj, "foo1", 10);

verifyWritable(obj, "foo1");

verifyEnumerable(obj, "foo1");

verifyNotConfigurable(obj, "foo1");
verifyEqualTo(obj, "foo2", get_func());

verifyWritable(obj, "foo2", "variableForHelpVerify");

verifyEnumerable(obj, "foo2");

verifyNotConfigurable(obj, "foo2");
