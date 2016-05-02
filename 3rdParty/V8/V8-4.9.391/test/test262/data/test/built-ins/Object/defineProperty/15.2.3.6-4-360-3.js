// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-360-3
description: >
    ES5 Attributes - Updating data property 'P' whose attributes are
    [[Writable]]: false, [[Enumerable]]: true, [[Configurable]]: true
    to an accessor property, 'O' is the global object (8.12.9 - step
    9.b.i)
includes:
    - propertyHelper.js
    - fnGlobalObject.js
---*/

var obj = fnGlobalObject();

    Object.defineProperty(obj, "prop", {
        value: 2010,
        writable: false,
        enumerable: true,
        configurable: true
    });
    var desc1 = Object.getOwnPropertyDescriptor(obj, "prop");

    function getFunc() {
        return 20;
    }
    Object.defineProperty(obj, "prop", {
        get: getFunc
    });
    var desc2 = Object.getOwnPropertyDescriptor(obj, "prop");

    assert(desc1.hasOwnProperty("value"));
    assert(desc2.hasOwnProperty("get"));
    assert.sameValue(desc2.enumerable, true);
    assert.sameValue(desc2.configurable, true);
    assert.sameValue(obj.prop, 20);
    assert.sameValue(typeof desc2.set, "undefined");
    assert.sameValue(desc2.get, getFunc);
