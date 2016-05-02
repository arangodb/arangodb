// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-1-10
description: Array.prototype.forEach applied to the Math object
---*/

        var result = false;

        function callbackfn(val, idx, obj) {
            result = ('[object Math]' === Object.prototype.toString.call(obj));
        }

            Math.length = 1;
            Math[0] = 1;
            Array.prototype.forEach.call(Math, callbackfn);

assert(result, 'result !== true');
