// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-9-c-ii-13
description: >
    Array.prototype.reduce - callbackfn is called with 4 formal
    parameter
---*/

        var result = false;
        function callbackfn(prevVal, curVal, idx, obj) {
            result = (prevVal === 1 && obj[idx] === curVal);
        }

        [11].reduce(callbackfn, 1);

assert(result, 'result !== true');
