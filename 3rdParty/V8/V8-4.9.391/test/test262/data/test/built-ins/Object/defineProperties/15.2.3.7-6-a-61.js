// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-61
description: >
    Object.defineProperties - both desc.enumerable and P.enumerable
    are boolean values with the same value (8.12.9 step 6)
includes: [propertyHelper.js]
---*/


var obj = {};

Object.defineProperty(obj, "foo", { 
    value: 10, 
    enumerable: false 
});

Object.defineProperties(obj, {
    foo: {
        enumerable: false
    }
});
verifyEqualTo(obj, "foo", 10);

verifyNotWritable(obj, "foo");

verifyNotEnumerable(obj, "foo");

verifyNotConfigurable(obj, "foo");
