// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-3-29
description: >
    Array.prototype.some - value of 'length' is boundary value (2^32 +
    1)
---*/

        function callbackfn1(val, idx, obj) {
            return val > 10;
        }

        function callbackfn2(val, idx, obj) {
            return val > 11;
        }

        var obj = {
            0: 11,
            1: 12,
            length: 4294967297
        };

assert(Array.prototype.some.call(obj, callbackfn1), 'Array.prototype.some.call(obj, callbackfn1) !== true');
assert(Array.prototype.some.call(obj, callbackfn2), 'Array.prototype.some.call(obj, callbackfn2) !== true');
