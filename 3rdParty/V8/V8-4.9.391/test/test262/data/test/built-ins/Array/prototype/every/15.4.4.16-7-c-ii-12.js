// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-7-c-ii-12
description: >
    Array.prototype.every - callbackfn is called with 3 formal
    parameter
---*/

        var called = 0;

        function callbackfn(val, idx, obj) {
            called++;
            return val > 10 && obj[idx] === val;
        }

assert([11, 12, 13].every(callbackfn), '[11, 12, 13].every(callbackfn) !== true');
assert.sameValue(called, 3, 'called');
