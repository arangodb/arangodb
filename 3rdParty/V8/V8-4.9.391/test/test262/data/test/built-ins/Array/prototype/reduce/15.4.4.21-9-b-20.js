// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-9-b-20
description: >
    Array.prototype.reduce - properties can be added to prototype are
    visited on an Array
---*/

        var testResult = false;

        function callbackfn(accum, val, idx, obj) {
            if (idx === 1 && val === 6.99) {
                testResult = true;
            }
        }

        var arr = [0, , 2];

        Object.defineProperty(arr, "0", {
            get: function () {
                Object.defineProperty(Array.prototype, "1", {
                    get: function () {
                        return 6.99;
                    },
                    configurable: true
                });
                return 0;
            },
            configurable: true
        });

            arr.reduce(callbackfn, "initialValue");

assert(testResult, 'testResult !== true');
