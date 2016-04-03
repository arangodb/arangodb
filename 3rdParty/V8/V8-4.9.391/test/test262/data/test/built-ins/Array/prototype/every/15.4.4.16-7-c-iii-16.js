// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-7-c-iii-16
description: >
    Array.prototype.every - return value of callbackfn is an Array
    object
---*/

        var accessed = false;

        function callbackfn(val, idx, obj) {
            accessed = true;
            return [];
        }

assert([11].every(callbackfn), '[11].every(callbackfn) !== true');
assert(accessed, 'accessed !== true');
