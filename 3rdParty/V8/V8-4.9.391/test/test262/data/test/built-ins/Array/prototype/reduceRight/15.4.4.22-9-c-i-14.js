// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-9-c-i-14
description: >
    Array.prototype.reduceRight - element to be retrieved is own
    accessor property that overrides an inherited accessor property on
    an Array
---*/

        var testResult = false;
        function callbackfn(prevVal, curVal, idx, obj) {
            if (idx === 1) {
                testResult = (curVal === "1");
            }
        }

            Object.defineProperty(Array.prototype, "1", {
                get: function () {
                    return 11;
                },
                configurable: true
            });

            var arr = [0, ,2];

            Object.defineProperty(arr, "1", {
                get: function () {
                    return "1";
                },
                configurable: true
            });
            arr.reduceRight(callbackfn, "initialValue");

assert(testResult, 'testResult !== true');
