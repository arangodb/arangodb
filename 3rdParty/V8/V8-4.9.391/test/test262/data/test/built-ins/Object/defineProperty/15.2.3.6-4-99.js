// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-99
description: >
    Object.defineProperty will throw TypeError when name.configurable
    = false, name.[[Get]] is undefined, desc.[[Get]] refers to an
    object (8.12.9 step 11.a.ii)
includes: [propertyHelper.js]
---*/


var obj = {};

function setFunc(value) {
    obj.setVerifyHelpProp = value;
}

Object.defineProperty(obj, "foo", {
    set: setFunc,
    configurable: false
});

function getFunc() {
    return 10;
}

try {
    Object.defineProperty(obj, "foo", {
        get: getFunc,
        set: setFunc
    });
    $ERROR("Expected an exception.");
} catch (e) {
    verifyWritable(obj, "foo", "setVerifyHelpProp");

    verifyNotEnumerable(obj, "foo");

    verifyNotConfigurable(obj, "foo");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
