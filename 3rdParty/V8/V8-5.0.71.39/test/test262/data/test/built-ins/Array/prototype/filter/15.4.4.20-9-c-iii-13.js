// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.20-9-c-iii-13
description: >
    Array.prototype.filter return value of callbackfn is a number
    (value is NaN)
---*/

        var accessed = false;

        function callbackfn(val, idx, obj) {
            accessed = true;
            return NaN;
        }

        var newArr = [11].filter(callbackfn);

assert.sameValue(newArr.length, 0, 'newArr.length');
assert(accessed, 'accessed !== true');
