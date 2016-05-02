// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-8-b-iii-1-22
description: >
    Array.prototype.reduceRight - element to be retrieved is inherited
    accessor property without a get function on an Array
---*/

        var testResult = false;
        function callbackfn(prevVal, curVal, idx, obj) {
            if (idx === 1) {
                testResult = (typeof prevVal === "undefined");
            }
        }

            Object.defineProperty(Array.prototype, "2", {
                set: function () { },
                configurable: true
            });

            var arr = [0, 1, , ];

            arr.reduceRight(callbackfn);

assert(testResult, 'testResult !== true');
