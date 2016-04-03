// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.20-2-18
description: >
    Array.prototype.filter applied to String object, which implements
    its own property get method
---*/

        function callbackfn(val, idx, obj) {
            return obj.length === 3;
        }

        var str = new String("012");

        var newArr = Array.prototype.filter.call(str, callbackfn);

assert.sameValue(newArr.length, 3, 'newArr.length');
