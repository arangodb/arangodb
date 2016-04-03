// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.20-9-c-i-23
description: >
    Array.prototype.filter - This object is the global object which
    contains index property
includes: [fnGlobalObject.js]
---*/

        function callbackfn(val, idx, obj) {
            return idx === 0 && val === 11;
        }

            var oldLen = fnGlobalObject().length;
            fnGlobalObject()[0] = 11;
            fnGlobalObject().length = 1;
            var newArr = Array.prototype.filter.call(fnGlobalObject(), callbackfn);

assert.sameValue(newArr.length, 1, 'newArr.length');
assert.sameValue(newArr[0], 11, 'newArr[0]');
