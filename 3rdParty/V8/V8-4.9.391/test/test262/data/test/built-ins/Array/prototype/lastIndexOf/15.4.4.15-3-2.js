// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.15-3-2
description: >
    Array.prototype.lastIndexOf return -1 when value of 'length' is a
    boolean (value is true)
---*/

        var obj = { 0: 0, 1: 1, length: true };

assert.sameValue(Array.prototype.lastIndexOf.call(obj, 0), 0, 'Array.prototype.lastIndexOf.call(obj, 0)');
assert.sameValue(Array.prototype.lastIndexOf.call(obj, 1), -1, 'Array.prototype.lastIndexOf.call(obj, 1)');
