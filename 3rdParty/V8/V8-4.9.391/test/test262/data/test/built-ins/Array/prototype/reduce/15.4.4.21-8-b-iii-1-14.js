// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-8-b-iii-1-14
description: >
    Array.prototype.reduce - element to be retrieved is own accessor
    property that overrides an inherited accessor property on an Array
---*/

        var testResult = false;
        function callbackfn(prevVal, curVal, idx, obj) {
            if (idx === 1) {
                testResult = (prevVal === "9");
            }
        }

            Object.defineProperty(Array.prototype, "0", {
                get: function () {
                    return 0;
                },
                configurable: true
            });

            var arr = [, 1, 2];
            Object.defineProperty(arr, "0", {
                get: function () {
                    return "9";
                },
                configurable: true
            });

            arr.reduce(callbackfn);

assert(testResult, 'testResult !== true');
