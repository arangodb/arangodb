// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-2-2
description: Array.prototype.forEach - 'length' is own data property on an Array
---*/

        var result = false;
        function callbackfn(val, idx, obj) {
            result = (obj.length === 2);
        }

        [12, 11].forEach(callbackfn);

assert(result, 'result !== true');
