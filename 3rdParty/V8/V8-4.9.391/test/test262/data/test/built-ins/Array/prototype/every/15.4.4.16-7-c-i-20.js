// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-7-c-i-20
description: >
    Array.prototype.every - element to be retrieved is own accessor
    property without a get function that overrides an inherited
    accessor property on an Array
---*/

        var accessed = false;

        function callbackfn(val, idx, obj) {
            accessed = true;
            return typeof val === "undefined";
        }

        var arr = [];

        Object.defineProperty(arr, "0", {
            set: function () { },
            configurable: true
        });

            Array.prototype[0] = 100;

assert(arr.every(callbackfn), 'arr.every(callbackfn) !== true');
assert(accessed, 'accessed !== true');
