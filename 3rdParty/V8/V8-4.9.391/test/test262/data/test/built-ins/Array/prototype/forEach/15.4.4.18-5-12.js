// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-5-12
description: Array.prototype.forEach - Boolean Object can be used as thisArg
---*/

        var result = false;
        var objBoolean = new Boolean();

        function callbackfn(val, idx, obj) {
            result = (this === objBoolean);
        }

        [11].forEach(callbackfn, objBoolean);

assert(result, 'result !== true');
