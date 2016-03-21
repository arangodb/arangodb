// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.20-5-12
description: Array.prototype.filter - Boolean Object can be used as thisArg
---*/

        var accessed = false;
        var objBoolean = new Boolean();

        function callbackfn(val, idx, obj) {
            accessed = true;
            return this === objBoolean;
        }

        var newArr = [11].filter(callbackfn, objBoolean);

assert.sameValue(newArr[0], 11, 'newArr[0]');
assert(accessed, 'accessed !== true');
