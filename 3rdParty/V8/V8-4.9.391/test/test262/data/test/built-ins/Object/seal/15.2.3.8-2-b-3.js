// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.8-2-b-3
description: >
    Object.seal - the [[Configurable]] attribute of all own properties
    of 'O' are set from true to false and other attributes of the
    property are unaltered
includes: [propertyHelper.js]
---*/

var obj = {};
obj.variableForHelpVerify = "data";

Object.defineProperty(obj, "foo1", {
    value: 10,
    writable: true,
    enumerable: true,
    configurable: true
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
    configurable: true
});
var preCheck = Object.isExtensible(obj);
Object.seal(obj);

if (!preCheck) {
    $ERROR('Expected preCheck to be true, actually ' + preCheck);
}


verifyEqualTo(obj, "foo1", 10);

verifyWritable(obj, "foo1");

verifyEnumerable(obj, "foo1");

verifyNotConfigurable(obj, "foo1");

verifyEqualTo(obj, "foo2", get_func());

verifyWritable(obj, "foo2", "variableForHelpVerify");

verifyEnumerable(obj, "foo2");

verifyNotConfigurable(obj, "foo2");
