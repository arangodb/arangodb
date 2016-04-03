// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-3-7
description: >
    Array.prototype.every - value of 'length' is a number (value is
    negative)
---*/

        function callbackfn1(val, idx, obj) {
            return val > 10;
        }

        function callbackfn2(val, idx, obj) {
            return val > 11;
        }

        var obj = { 0: 12, 1: 11, 2: 9, length: -4294967294 }; //length used to exec while loop is 0

assert(Array.prototype.every.call(obj, callbackfn1), 'Array.prototype.every.call(obj, callbackfn1) !== true');
assert(Array.prototype.every.call(obj, callbackfn2), 'Array.prototype.every.call(obj, callbackfn2) !== true');
