// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-9-c-ii-18
description: >
    Array.prototype.reduce - value of 'accumulator' used for first
    iteration is the value of 'initialValue' when it is present on an
    Array-like object
---*/

        var result = false;
        function callbackfn(prevVal, curVal, idx, obj) {
            if (idx === 0) {
                result = (arguments[0] === 1);
            }
        }

        var obj = { 0: 11, 1: 9, length: 2 };

        Array.prototype.reduce.call(obj, callbackfn, 1);

assert(result, 'result !== true');
