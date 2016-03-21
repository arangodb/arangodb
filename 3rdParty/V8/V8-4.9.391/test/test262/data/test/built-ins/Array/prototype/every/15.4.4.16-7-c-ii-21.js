// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-7-c-ii-21
description: >
    Array.prototype.every - callbackfn called with correct parameters
    (kValue is correct)
---*/

        var accessed = false;

        function callbackfn(val, idx, obj) {
            accessed = true;
            if (idx === 0) {
                return val === 11;
            }

            if (idx === 1) {
                return val === 12;
            }

        }

        var obj = { 0: 11, 1: 12, length: 2 };

assert(Array.prototype.every.call(obj, callbackfn), 'Array.prototype.every.call(obj, callbackfn) !== true');
assert(accessed, 'accessed !== true');
