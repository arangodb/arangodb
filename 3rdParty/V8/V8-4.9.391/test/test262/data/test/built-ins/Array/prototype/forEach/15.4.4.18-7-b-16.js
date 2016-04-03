// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-7-b-16
description: >
    Array.prototype.forEach - decreasing length of array does not
    delete non-configurable properties
flags: [noStrict]
---*/

        var testResult = false;

        function callbackfn(val, idx, obj) {
            if (idx === 2 && val === "unconfigurable") {
                testResult = true;
            }
        }

        var arr = [0, 1, 2];

        Object.defineProperty(arr, "2", {
            get: function () {
                return "unconfigurable";
            },
            configurable: false
        });

        Object.defineProperty(arr, "1", {
            get: function () {
                arr.length = 2;
                return 1;
            },
            configurable: true
        });

        arr.forEach(callbackfn);

assert(testResult, 'testResult !== true');
