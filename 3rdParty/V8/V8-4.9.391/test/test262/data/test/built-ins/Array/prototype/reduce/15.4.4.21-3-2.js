// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-3-2
description: >
    Array.prototype.reduce - value of 'length' is a boolean (value is
    true)
---*/

        function callbackfn(prevVal, curVal, idx, obj) {
            return (curVal === 11 && idx === 0);
        }

        var obj = { 0: 11, 1: 9, length: true };

assert.sameValue(Array.prototype.reduce.call(obj, callbackfn, 1), true, 'Array.prototype.reduce.call(obj, callbackfn, 1)');
