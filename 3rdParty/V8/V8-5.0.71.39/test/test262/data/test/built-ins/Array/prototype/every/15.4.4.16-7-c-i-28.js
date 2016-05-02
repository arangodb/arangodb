// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-7-c-i-28
description: >
    Array.prototype.every - element changed by getter on previous
    iterations is observed on an Array
---*/

        var preIterVisible = false;
        var arr = [];

        function callbackfn(val, idx, obj) {
            return val > 10;
        }

        Object.defineProperty(arr, "0", {
            get: function () {
                preIterVisible = true;
                return 11;
            },
            configurable: true
        });

        Object.defineProperty(arr, "1", {
            get: function () {
                if (preIterVisible) {
                    return 9;
                } else {
                    return 11;
                }
            },
            configurable: true
        });

assert.sameValue(arr.every(callbackfn), false, 'arr.every(callbackfn)');
