// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.9-2-a-14
description: >
    Object.freeze - 'P' is own index property of an Array object that
    uses Object's [[GetOwnProperty]]
includes: [propertyHelper.js]
---*/

// default [[Configurable]] attribute value of "0": true
var arrObj = [0, 1, 2];

Object.freeze(arrObj);

verifyNotWritable(arrObj, "0");
verifyNotConfigurable(arrObj, "0");
assert.sameValue(arrObj[0], 0);
