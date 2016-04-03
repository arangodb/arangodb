// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-7-c-ii-12
description: >
    Array.prototype.forEach - callbackfn is called with 3 formal
    parameter
---*/

        var result = false;
        function callbackfn(val, idx, obj) {
            result = (val > 10 && obj[idx] === val);
        }

        [11].forEach(callbackfn);

assert(result, 'result !== true');
