// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.9-2-c-2
description: >
    Object.freeze - The [[Configurable]] attribute of own accessor
    property of 'O' is set to false while other attributes are
    unchanged
includes: [propertyHelper.js]
---*/

var obj = {};

function get_func() {
    return 10;
}

var set_funcCalled = false;
function set_func() {
    set_funcCalled = true;
}

Object.defineProperty(obj, "foo", {
    get: get_func,
    set: set_func,
    enumerable: true,
    configurable: true
});

Object.freeze(obj);

assert(obj.hasOwnProperty("foo"));
verifyNotConfigurable(obj, "foo");

assert.sameValue(obj.foo, 10);

obj.foo = 12;
assert(set_funcCalled);

verifyEnumerable(obj, "foo");

var desc = Object.getOwnPropertyDescriptor(obj, "foo");
assert.sameValue(desc.configurable, false);
