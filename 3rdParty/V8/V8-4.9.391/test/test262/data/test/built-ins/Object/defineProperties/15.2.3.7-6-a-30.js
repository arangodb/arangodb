// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-30
description: >
    Object.defineProperties - 'P' doesn't exist in 'O', test
    [[Configurable]] of 'P' is set as false value if absent in data
    descriptor 'desc' (8.12.9 step 4.a.i)
includes: [propertyHelper.js]
---*/

var obj = {};


Object.defineProperties(obj, {
    prop: { value: 1001 }
});

verifyNotConfigurable(obj, "prop");

if (!obj.hasOwnProperty("prop")) {
    $ERROR('Expected obj.hasOwnProperty("prop") to be true, actually ' + obj.hasOwnProperty("prop"));
}

if (obj.prop !== 1001) {
    $ERROR('Expected obj.prop === 1001, actually ' + obj.prop);
}

