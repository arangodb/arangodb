// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-7-c-i-6
description: >
    Array.prototype.every - element to be retrieved is own data
    property that overrides an inherited accessor property on an Array
---*/

        var accessed = false;

        function callbackfn(val, idx, obj) {
            accessed = true;
            return val === 11;
        }

            Object.defineProperty(Array.prototype, "0", {
                get: function () {
                    return 9;
                },
                configurable: true
            });

assert([11].every(callbackfn), '[11].every(callbackfn) !== true');
assert(accessed, 'accessed !== true');
