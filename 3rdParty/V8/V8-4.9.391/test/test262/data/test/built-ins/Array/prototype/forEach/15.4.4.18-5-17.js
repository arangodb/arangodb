// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-5-17
description: Array.prototype.forEach - the JSON object can be used as thisArg
---*/

        var result = false;
        function callbackfn(val, idx, obj) {
            result = (this === JSON);
        }

        [11].forEach(callbackfn, JSON);

assert(result, 'result !== true');
