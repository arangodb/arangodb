// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-5-18
description: Array.prototype.forEach - Error Object can be used as thisArg
---*/

        var result = false;
        var objError = new RangeError();

        function callbackfn(val, idx, obj) {
            result = (this === objError);
        }

        [11].forEach(callbackfn, objError);

assert(result, 'result !== true');
