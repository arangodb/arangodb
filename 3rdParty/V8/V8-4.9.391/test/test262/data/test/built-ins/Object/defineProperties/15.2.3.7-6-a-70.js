// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-70
description: >
    Object.defineProperties - 'P' is accessor property and
    P.configurable is true, 'desc' in 'Properties' is data property
    (8.12.9 step 9.c.i)
includes: [propertyHelper.js]
---*/


var obj = {};

function get_func() {
    return 10;
}

Object.defineProperty(obj, "foo", {
    get: get_func,
    configurable: true
});

Object.defineProperties(obj, {
    foo: {
        value: 12
    }
});
verifyEqualTo(obj, "foo", 12);

verifyNotWritable(obj, "foo");

verifyNotEnumerable(obj, "foo");

verifyConfigurable(obj, "foo");
