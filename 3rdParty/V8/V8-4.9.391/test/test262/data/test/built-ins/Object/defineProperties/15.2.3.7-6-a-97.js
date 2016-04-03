// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-97
description: >
    Object.defineProperties - 'P' is data property, P.writable and
    properties.writable are different values (8.12.9 step 12)
includes: [propertyHelper.js]
---*/


var obj = {};

Object.defineProperty(obj, "foo", { 
    value: 100, 
    enumerable: true, 
    writable: false, 
    configurable: true 
});

Object.defineProperties(obj, {
    foo: {
        writable: true
    }
});
verifyEqualTo(obj, "foo", 100);

verifyWritable(obj, "foo");

verifyEnumerable(obj, "foo");

verifyConfigurable(obj, "foo");
