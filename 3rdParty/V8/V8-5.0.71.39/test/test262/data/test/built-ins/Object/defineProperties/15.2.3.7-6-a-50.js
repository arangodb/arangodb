// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-50
description: >
    Object.defineProperties - desc.value and P.value are two strings
    with different values  (8.12.9 step 6)
includes: [propertyHelper.js]
---*/


var obj = {};

obj.foo = "abcd"; // default value of attributes: writable: true, configurable: true, enumerable: true

Object.defineProperties(obj, {
    foo: {
        value: "fghj"
    }
});
verifyEqualTo(obj, "foo", "fghj");

verifyWritable(obj, "foo");

verifyEnumerable(obj, "foo");

verifyConfigurable(obj, "foo");
