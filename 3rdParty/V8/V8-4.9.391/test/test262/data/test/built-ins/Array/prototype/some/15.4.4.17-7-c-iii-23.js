// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-7-c-iii-23
description: >
    Array.prototype.some - return value of callbackfn is the JSON
    object
---*/

        function callbackfn(val, idx, obj) {
            return JSON;
        }

assert([11].some(callbackfn), '[11].some(callbackfn) !== true');
