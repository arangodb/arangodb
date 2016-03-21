// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.20-5-13
description: Array.prototype.filter - Number Object can be used as thisArg
---*/

        var accessed = false;
        var objNumber = new Number();

        function callbackfn(val, idx, obj) {
            accessed = true;
            return this === objNumber;
        }

        var newArr = [11].filter(callbackfn, objNumber);

assert.sameValue(newArr[0], 11, 'newArr[0]');
assert(accessed, 'accessed !== true');
