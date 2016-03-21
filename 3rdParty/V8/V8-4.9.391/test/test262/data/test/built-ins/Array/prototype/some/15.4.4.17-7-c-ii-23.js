// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-7-c-ii-23
description: >
    Array.prototype.some - callbackfn called with correct parameters
    (this object O is correct)
---*/

        var obj = { 0: 11, 1: 12, length: 2 };

        function callbackfn(val, idx, o) {
            return obj === o;
        }

assert(Array.prototype.some.call(obj, callbackfn), 'Array.prototype.some.call(obj, callbackfn) !== true');
