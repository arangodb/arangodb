// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-7-c-i-23
description: >
    Array.prototype.every - This object is an global object which
    contains index property
includes: [fnGlobalObject.js]
---*/

        function callbackfn(val, idx, obj) {
            if (idx === 0) {
                return val !== 11;
            } else {
                return true;
            }
        }

            var oldLen = fnGlobalObject().length;
            fnGlobalObject()[0] = 11;
            fnGlobalObject().length = 1;

assert.sameValue(Array.prototype.every.call(fnGlobalObject(), callbackfn), false, 'Array.prototype.every.call(fnGlobalObject(), callbackfn)');
