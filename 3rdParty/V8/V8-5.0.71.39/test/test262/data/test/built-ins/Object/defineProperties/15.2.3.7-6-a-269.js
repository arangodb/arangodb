// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-269
description: >
    Object.defineProperties - 'O' is an Array, 'P' is generic own data
    property of 'O', and 'desc' is data descriptor, test updating
    multiple attribute values of 'P' (15.4.5.1 step 5)
includes: [propertyHelper.js]
---*/


var arr = [];
arr.property = 12; // default value of attributes: writable: true, configurable: true, enumerable: true

Object.defineProperties(arr, {
    "property": {
        writable: false,
        enumerable: false,
        configurable: false
    }
});
verifyEqualTo(arr, "property", 12);

verifyNotWritable(arr, "property");

verifyNotEnumerable(arr, "property");

verifyNotConfigurable(arr, "property");

if (arr.length !== 0) {
    $ERROR('Expected arr.length === 0, actually ' + arr.length);
}

