// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-8-c-iii-2
description: >
    Array.prototype.map - value of returned array element equals to
    'mappedValue'
---*/

        function callbackfn(val, idx, obj) {
            return val;
        }

        var obj = { 0: 11, 1: 9, length: 2 };
        var newArr = Array.prototype.map.call(obj, callbackfn);

assert.sameValue(newArr[0], obj[0], 'newArr[0]');
assert.sameValue(newArr[1], obj[1], 'newArr[1]');
