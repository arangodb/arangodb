// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-7-c-iii-29
description: >
    Array.prototype.some - return value (new Boolean(false)) of
    callbackfn is treated as true value
---*/

        function callbackfn() {
            return new Boolean(false);
        }

assert([11].some(callbackfn), '[11].some(callbackfn) !== true');
