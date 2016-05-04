// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-7-c-i-22
description: >
    Array.prototype.every - element to be retrieved is inherited
    accessor property without a get function on an Array
---*/

        var accessed = false;

        function callbackfn(val, idx, obj) {
            accessed = true;
            return typeof val === "undefined";
        }

            Object.defineProperty(Array.prototype, "0", {
                set: function () { },
                configurable: true
            });

assert([, ].every(callbackfn), '[, ].every(callbackfn) !== true');
assert(accessed, 'accessed !== true');
