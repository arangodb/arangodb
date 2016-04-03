// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.20-1-11
description: Array.prototype.filter applied to Date object
---*/

        function callbackfn(val, idx, obj) {
            return obj instanceof Date;
        }

        var obj = new Date();
        obj.length = 1;
        obj[0] = 1;

        var newArr = Array.prototype.filter.call(obj, callbackfn);

assert.sameValue(newArr[0], 1, 'newArr[0]');
