// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-1-11
description: Array.prototype.every applied to Date object
---*/

        function callbackfn(val, idx, obj) {
            return !(obj instanceof Date);
        }

        var obj = new Date();
        obj.length = 1;
        obj[0] = 1;

assert.sameValue(Array.prototype.every.call(obj, callbackfn), false, 'Array.prototype.every.call(obj, callbackfn)');
