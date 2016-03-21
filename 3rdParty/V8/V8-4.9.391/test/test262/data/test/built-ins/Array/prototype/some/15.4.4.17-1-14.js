// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-1-14
description: Array.prototype.some applied to Error object
---*/

        function callbackfn(val, idx, obj) {
            return obj instanceof Error;
        }

        var obj = new Error();
        obj.length = 1;
        obj[0] = 1;

assert(Array.prototype.some.call(obj, callbackfn), 'Array.prototype.some.call(obj, callbackfn) !== true');
