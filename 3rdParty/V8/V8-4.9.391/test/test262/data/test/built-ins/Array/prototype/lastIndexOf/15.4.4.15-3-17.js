// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.15-3-17
description: >
    Array.prototype.lastIndexOf - value of 'length' is a string
    containing a number with leading zeros
---*/

        var obj = { 1: 1, 2: 2, length: "0002.0" };

assert.sameValue(Array.prototype.lastIndexOf.call(obj, 1), 1, 'Array.prototype.lastIndexOf.call(obj, 1)');
assert.sameValue(Array.prototype.lastIndexOf.call(obj, 2), -1, 'Array.prototype.lastIndexOf.call(obj, 2)');
