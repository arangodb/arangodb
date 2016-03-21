// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-3-9
description: >
    Array.prototype.map - value of 'length' is a number (value is
    -Infinity)
---*/

        function callbackfn(val, idx, obj) {
            return val < 10;
        }

        var obj = { 0: 9, length: -Infinity };

        var newArr = Array.prototype.map.call(obj, callbackfn);

assert.sameValue(newArr.length, 0, 'newArr.length');
