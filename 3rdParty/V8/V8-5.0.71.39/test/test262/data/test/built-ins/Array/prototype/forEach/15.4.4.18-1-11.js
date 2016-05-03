// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-1-11
description: Array.prototype.forEach applied to Date object
---*/

        var result = false;
        function callbackfn(val, idx, obj) {
            result = obj instanceof Date;
        }

        var obj = new Date();
        obj.length = 1;
        obj[0] = 1;

        Array.prototype.forEach.call(obj, callbackfn);

assert(result, 'result !== true');
