// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-88
description: >
    Object.defineProperties throws TypeError when P.configurable is
    false, P.[[Set]] is undefined, properties.[[Set]] refers to an
    object (8.12.9 step 11.a.i)
includes: [propertyHelper.js]
---*/


var obj = {};

function get_Func() {
    return 0;
}

Object.defineProperty(obj, "foo", {
    set: undefined,
    get: get_Func,
    enumerable: false,
    configurable: false
});

function set_Func() {}

try {
    Object.defineProperties(obj, {
        foo: {
            set: set_Func
        }
    });
    $ERROR("Expected an exception.");
} catch (e) {
    verifyNotEnumerable(obj, "foo");

    verifyNotConfigurable(obj, "foo");
    var desc = Object.getOwnPropertyDescriptor(obj, "foo");

    if (typeof (desc.set) !== "undefined") {
        $ERROR('Expected typeof (desc.set) === "undefined", actually ' + typeof (desc.set));
    }


    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
