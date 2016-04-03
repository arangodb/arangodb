// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.20-1-8
description: Array.prototype.filter applied to String object
---*/

        function callbackfn(val, idx, obj) {
            return obj instanceof String;
        }

        var obj = new String("abc");
        var newArr = Array.prototype.filter.call(obj, callbackfn);

assert.sameValue(newArr[0], "a", 'newArr[0]');
