// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.20-9-c-iii-1-4
description: >
    Array.prototype.filter - value of returned array element can be
    changed or deleted
---*/

        function callbackfn(val, idx, obj) {
            return true;
        }

        var obj = { 0: 11, 1: 9, length: 2 };
        var newArr = Array.prototype.filter.call(obj, callbackfn);

            var tempVal = newArr[1];
            delete newArr[1];

assert.notSameValue(tempVal, undefined, 'tempVal');
assert.sameValue(newArr[1], undefined, 'newArr[1]');
