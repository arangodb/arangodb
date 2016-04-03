// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.20-5-21
description: Array.prototype.filter - the global object can be used as thisArg
includes: [fnGlobalObject.js]
---*/

        var accessed = false;

        function callbackfn(val, idx, obj) {
            accessed = true;
            return this === fnGlobalObject();
        }

        var newArr = [11].filter(callbackfn, fnGlobalObject());

assert.sameValue(newArr[0], 11, 'newArr[0]');
assert(accessed, 'accessed !== true');
