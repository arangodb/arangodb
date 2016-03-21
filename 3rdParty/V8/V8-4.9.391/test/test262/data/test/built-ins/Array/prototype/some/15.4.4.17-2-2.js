// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-2-2
description: Array.prototype.some - 'length' is own data property on an Array
---*/

        function callbackfn1(val, idx, obj) {
            return val > 10;
        }

        function callbackfn2(val, idx, obj) {
            return val > 11;
        }

            Array.prototype[2] = 12;

assert([9, 11].some(callbackfn1), '[9, 11].some(callbackfn1) !== true');
assert.sameValue([9, 11].some(callbackfn2), false, '[9, 11].some(callbackfn2)');
