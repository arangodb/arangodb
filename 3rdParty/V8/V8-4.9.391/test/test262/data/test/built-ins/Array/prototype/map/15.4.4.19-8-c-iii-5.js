// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-8-c-iii-5
description: >
    Array.prototype.map - value of returned array element can be
    changed or deleted
---*/

        function callbackfn(val, idx, obj) {
            return true;
        }

        var obj = { 0: 11, 1: 9, length: 2 };
        var newArr = Array.prototype.map.call(obj, callbackfn);

            var tempVal = newArr[1];
            delete newArr[1];

assert.notSameValue(tempVal, undefined, 'tempVal');
assert.sameValue(newArr[1], undefined, 'newArr[1]');
