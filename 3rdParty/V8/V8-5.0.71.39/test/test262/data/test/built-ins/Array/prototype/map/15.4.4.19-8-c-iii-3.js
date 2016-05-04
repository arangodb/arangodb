// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-8-c-iii-3
description: >
    Array.prototype.map - value of returned array element can be
    overwritten
---*/

        function callbackfn(val, idx, obj) {
            return 11;
        }

        var obj = { 0: 11, 1: 9, length: 2 };
        var newArr = Array.prototype.map.call(obj, callbackfn);

            var tempVal = newArr[1];
            newArr[1] += 1;

assert.notSameValue(newArr[1], tempVal, 'newArr[1]');
