// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.20-9-c-ii-11
description: >
    Array.prototype.filter - callbackfn is called with 2 formal
    parameter
---*/

        function callbackfn(val, idx) {
            return val > 10 && arguments[2][idx] === val;
        }
        var newArr = [11].filter(callbackfn);

assert.sameValue(newArr.length, 1, 'newArr.length');
assert.sameValue(newArr[0], 11, 'newArr[0]');
