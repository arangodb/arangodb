// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-5-11
description: Array.prototype.forEach - String Object can be used as thisArg
---*/

        var result = false;
        var objString = new String();

        function callbackfn(val, idx, obj) {
            result = (this === objString);
        }

        [11].forEach(callbackfn, objString);

assert(result, 'result !== true');
