// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-5-16
description: Array.prototype.some - RegExp object can be used as thisArg
---*/

        var objRegExp = new RegExp();

        function callbackfn(val, idx, obj) {
            return this === objRegExp;
        }

assert([11].some(callbackfn, objRegExp), '[11].some(callbackfn, objRegExp) !== true');
