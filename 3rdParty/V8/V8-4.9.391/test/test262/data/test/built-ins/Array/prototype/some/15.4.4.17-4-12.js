// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-4-12
description: Array.prototype.some - 'callbackfn' is a function
---*/

        function callbackfn(val, idx, obj) {
            return val > 10;
        }

assert([9, 11].some(callbackfn), '[9, 11].some(callbackfn) !== true');
