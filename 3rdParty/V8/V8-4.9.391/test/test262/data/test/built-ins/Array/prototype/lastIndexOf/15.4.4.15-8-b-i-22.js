// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.15-8-b-i-22
description: >
    Array.prototype.lastIndexOf - element to be retrieved is inherited
    accessor property without a get function on an Array-like object
---*/

            Object.defineProperty(Object.prototype, "0", {
                set: function () { },
                configurable: true
            });

assert.sameValue(Array.prototype.lastIndexOf.call({ length: 1 }, undefined), 0, 'Array.prototype.lastIndexOf.call({ length: 1 }, undefined)');
