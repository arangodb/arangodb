// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-354-8
description: >
    ES5 Attributes - property 'P' with attributes [[Writable]]: false,
    [[Enumerable]]: true, [[Configurable]]: true is non-writable using
    simple assignment, 'O' is the global object
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

    assert.sameValue(obj.prop, 2010);
    verifyNotWritable(obj, "prop");
    assert.sameValue(obj.prop, 2010);
