// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-210
description: >
    Object.defineProperty - 'O' is an Array, 'name' is an array index
    named property, 'name' makes no change if every field in 'desc' is
    absent (name is data property) (15.4.5.1 step 4.c)
includes: [propertyHelper.js]
---*/

var arrObj = [];

arrObj[0] = 101; // default value of attributes: writable: true, configurable: true, enumerable: true

Object.defineProperty(arrObj, "0", {});
verifyEqualTo(arrObj, "0", 101);

verifyWritable(arrObj, "0");

verifyEnumerable(arrObj, "0");

verifyConfigurable(arrObj, "0");

