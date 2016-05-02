// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-7-b-14
description: >
    Array.prototype.some - decreasing length of array causes index
    property not to be visited
---*/

        var accessed = false;
        function callbackfn(val, idx, obj) {
            accessed = true;
            return idx === 3;
        }
        var arr = [0, 1, 2, "last"];

        Object.defineProperty(arr, "0", {
            get: function () {
                arr.length = 3;
                return 0;
            },
            configurable: true
        });

assert.sameValue(arr.some(callbackfn), false, 'arr.some(callbackfn)');
assert(accessed, 'accessed !== true');
