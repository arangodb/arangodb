// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-2-15
description: Array.prototype.forEach - 'length' is property of the global object
includes: [fnGlobalObject.js]
---*/

        var result = false;
        function callbackfn(val, idx, obj) {
            result = (obj.length === 2);
        }

            var oldLen = fnGlobalObject().length;
            fnGlobalObject()[0] = 12;
            fnGlobalObject()[1] = 11;
            fnGlobalObject()[2] = 9;
            fnGlobalObject().length = 2;
            Array.prototype.forEach.call(fnGlobalObject(), callbackfn);

assert(result, 'result !== true');
