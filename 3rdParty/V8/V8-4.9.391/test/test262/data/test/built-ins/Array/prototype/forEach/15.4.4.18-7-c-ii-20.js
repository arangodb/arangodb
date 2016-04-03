// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-7-c-ii-20
description: >
    Array.prototype.forEach - callbackfn called with correct
    parameters (thisArg is correct)
---*/

        var result = false;
        function callbackfn(val, idx, obj) {
            result = (10 === this.threshold);
        }

        var thisArg = { threshold: 10 };

        var obj = { 0: 11, length: 1 };

        Array.prototype.forEach.call(obj, callbackfn, thisArg);

assert(result, 'result !== true');
