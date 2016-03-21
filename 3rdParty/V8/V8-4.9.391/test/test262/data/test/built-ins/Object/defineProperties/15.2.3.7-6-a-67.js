// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-67
description: >
    Object.defineProperties throws TypeError when P is accessor
    property and P.configurable is false, desc is data property
    (8.12.9 step 9.a)
includes: [propertyHelper.js]
---*/

var obj = {};

function get_Func() {
    return 10;
}

Object.defineProperty(obj, "foo", {
    get: get_Func,
    configurable: false
});

try {
    Object.defineProperties(obj, {
        foo: {
            value: 11
        }
    });

    $ERROR("Expected TypeError");
} catch (e) {
    assert(e instanceof TypeError);
    verifyNotEnumerable(obj, "foo");

    assert.sameValue(obj.foo, 10);

    verifyNotConfigurable(obj, "foo");

    var desc = Object.getOwnPropertyDescriptor(obj, "foo");

    assert.sameValue(typeof (desc.set), "undefined");
    assert.sameValue(desc.get, get_Func);
}

