// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-7-c-i-23
description: >
    Array.prototype.some - This object is an global object which
    contains index property
includes: [fnGlobalObject.js]
---*/

        function callbackfn(val, idx, obj) {
            if (idx === 0) {
                return val === 11;
            }
            return false;
        }

            var oldLen = fnGlobalObject().length;
            fnGlobalObject()[0] = 11;
            fnGlobalObject().length = 1;

assert(Array.prototype.some.call(fnGlobalObject(), callbackfn), 'Array.prototype.some.call(fnGlobalObject(), callbackfn) !== true');
