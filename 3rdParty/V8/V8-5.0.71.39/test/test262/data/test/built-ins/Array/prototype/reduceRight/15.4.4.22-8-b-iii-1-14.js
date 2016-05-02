// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-8-b-iii-1-14
description: >
    Array.prototype.reduceRight - element to be retrieved is own
    accessor property that overrides an inherited accessor property on
    an Array
---*/

        var testResult = false;
        function callbackfn(prevVal, curVal, idx, obj) {
            if (idx === 1) {
                testResult = (prevVal === "20");
            }
        }

            Object.defineProperty(Array.prototype, "2", {
                get: function () {
                    return 2;
                },
                configurable: true
            });

            var arr = [0, 1, , ];

            Object.defineProperty(arr, "2", {
                get: function () {
                    return "20";
                },
                configurable: true
            });

            arr.reduceRight(callbackfn);

assert(testResult, 'testResult !== true');
