// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-7-c-i-29
description: >
    Array.prototype.some - element changed by getter on previous
    iterations on an Array-like object
---*/

        function callbackfn(val, idx, obj) {
            if (idx === 1) {
                return val === 12;
            }
            return false;
        }

        var obj = { length: 2 };
        var helpVerifyVar = 11;

        Object.defineProperty(obj, "1", {
            get: function () {
                return helpVerifyVar;
            },
            set: function (args) {
                helpVerifyVar = args;
            },
            configurable: true
        });

        Object.defineProperty(obj, "0", {
            get: function () {
                obj[1] = 12;
                return 11;
            },
            configurable: true
        });

assert(Array.prototype.some.call(obj, callbackfn), 'Array.prototype.some.call(obj, callbackfn) !== true');
