// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-5-21
description: Array.prototype.forEach - the global object can be used as thisArg
includes: [fnGlobalObject.js]
---*/

        var result = false;
        function callbackfn(val, idx, obj) {
            result = (this === fnGlobalObject());
        }

        [11].forEach(callbackfn, fnGlobalObject());

assert(result, 'result !== true');
