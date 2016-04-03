// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.15-3-24
description: >
    Array.prototype.lastIndexOf - value of 'length' is a positive
    non-integer, ensure truncation occurs in the proper direction
---*/

        var obj = { 122: true, 123: false, length: 123.5 };

assert.sameValue(Array.prototype.lastIndexOf.call(obj, true), 122, 'Array.prototype.lastIndexOf.call(obj, true)');
assert.sameValue(Array.prototype.lastIndexOf.call(obj, false), -1, 'Array.prototype.lastIndexOf.call(obj, false)');
