// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-7-c-i-18
description: >
    Array.prototype.forEach - element to be retrieved is own accessor
    property without a get function on an Array
---*/

        var testResult = false;

        function callbackfn(val, idx, obj) {
            if (idx === 0) {
                testResult = (typeof val === "undefined");
            }
        }

        var arr = [];

        Object.defineProperty(arr, "0", {
            set: function () { },
            configurable: true
        });

        arr.forEach(callbackfn);

assert(testResult, 'testResult !== true');
