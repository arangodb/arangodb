// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-71
description: >
    Object.defineProperties throws TypeError when 'P' is data property
    and  P.configurable is false, P.writable is false, desc is data
    property and  desc.writable is true (8.12.9 step 10.a.i)
includes: [propertyHelper.js]
---*/


var obj = {};

Object.defineProperty(obj, "foo", { 
    value: 10, 
    writable: false, 
    configurable: false 
});

try {
    Object.defineProperties(obj, {
        foo: {
            writable: true
        }
    });
    $ERROR("Expected an exception.");
} catch (e) {
    verifyEqualTo(obj, "foo", 10);

    verifyNotWritable(obj, "foo");

    verifyNotEnumerable(obj, "foo");

    verifyNotConfigurable(obj, "foo");

    if (!(e instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + e);
    }

}
