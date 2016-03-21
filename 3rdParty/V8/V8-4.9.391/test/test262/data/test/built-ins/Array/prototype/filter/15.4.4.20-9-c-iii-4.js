// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.20-9-c-iii-4
description: >
    Array.prototype.filter - return value of callbackfn is a boolean
    (value is false)
---*/

        var accessed = false;

        function callbackfn(val, idx, obj) {
            accessed = true;
            return false;
        }

        var obj = { 0: 11, length: 1 };

        var newArr = Array.prototype.filter.call(obj, callbackfn);

assert.sameValue(newArr.length, 0, 'newArr.length');
assert(accessed, 'accessed !== true');
