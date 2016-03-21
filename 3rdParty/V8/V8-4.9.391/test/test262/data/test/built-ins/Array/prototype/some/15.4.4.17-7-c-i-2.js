// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-7-c-i-2
description: >
    Array.prototype.some - element to be retrieved is own data
    property on an Array
---*/

        var kValue = {};

        function callbackfn(val, idx, obj) {
            if (idx === 0) {
                return kValue === val;
            }
            return false;
        }

assert([kValue].some(callbackfn), '[kValue].some(callbackfn) !== true');
