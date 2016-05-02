// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-5-12
description: Array.prototype.some - Boolean object can be used as thisArg
---*/

        var objBoolean = new Boolean();

        function callbackfn(val, idx, obj) {
            return this === objBoolean;
        }

assert([11].some(callbackfn, objBoolean), '[11].some(callbackfn, objBoolean) !== true');
