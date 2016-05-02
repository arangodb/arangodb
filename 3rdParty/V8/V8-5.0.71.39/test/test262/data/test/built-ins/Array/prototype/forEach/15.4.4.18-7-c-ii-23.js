// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-7-c-ii-23
description: >
    Array.prototype.forEach - callbackfn called with correct
    parameters (this object O is correct)
---*/

        var result = false;
        var obj = { 0: 11, length: 2 };

        function callbackfn(val, idx, o) {
            result = (obj === o);
        }

        Array.prototype.forEach.call(obj, callbackfn);

assert(result, 'result !== true');
