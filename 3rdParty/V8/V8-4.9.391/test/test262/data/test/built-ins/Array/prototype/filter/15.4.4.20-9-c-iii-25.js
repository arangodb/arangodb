// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.20-9-c-iii-25
description: >
    Array.prototype.filter - return value of callbackfn is an Error
    object
---*/

        function callbackfn(val, idx, obj) {
            return new EvalError();
        }

        var newArr = [11].filter(callbackfn);

assert.sameValue(newArr.length, 1, 'newArr.length');
assert.sameValue(newArr[0], 11, 'newArr[0]');
