// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-9-c-i-22
description: >
    Array.prototype.reduceRight - element to be retrieved is inherited
    accessor property without a get function on an Array
---*/

        var testResult = false;
        function callbackfn(prevVal, curVal, idx, obj) {
            if (idx === 1) {
                testResult = (typeof curVal === "undefined");
            }
        }

            Object.defineProperty(Array.prototype, "1", {
                set: function () { },
                configurable: true
            });

            var arr = [0, , 2];

            arr.reduceRight(callbackfn, "initialValue");

assert(testResult, 'testResult !== true');
