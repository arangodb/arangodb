// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-7-c-i-9
description: >
    Array.prototype.some - element to be retrieved is own accessor
    property on an Array-like object
---*/

        var kValue = "abc";

        function callbackfn(val, idx, obj) {
            if (idx === 10) {
                return val === kValue;
            }
            return false;
        }

        var obj = { length: 20 };

        Object.defineProperty(obj, "10", {
            get: function () {
                return kValue;
            },
            configurable: true
        });

assert(Array.prototype.some.call(obj, callbackfn), 'Array.prototype.some.call(obj, callbackfn) !== true');
