// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-7-c-iii-14
description: >
    Array.prototype.some - return value of callbackfn is a non-empty
    string
---*/

        function callbackfn(val, idx, obj) {
            return "non-empty string";
        }

assert([11].some(callbackfn), '[11].some(callbackfn) !== true');
