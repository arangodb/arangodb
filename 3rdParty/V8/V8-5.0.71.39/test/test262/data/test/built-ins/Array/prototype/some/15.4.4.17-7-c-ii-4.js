// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-7-c-ii-4
description: >
    Array.prototype.some - k values are passed in ascending numeric
    order
---*/

        var arr = [0, 1, 2, 3, 4, 5];
        var lastIdx = 0;
        var called = 0;

        function callbackfn(val, idx, o) {
            called++;
            if (lastIdx !== idx) {
                return true;
            } else {
                lastIdx++;
                return false;
            }
        }

assert.sameValue(arr.some(callbackfn), false, 'arr.some(callbackfn)');
assert.sameValue(arr.length, called, 'arr.length');
