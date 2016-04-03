// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.20-1-7
description: Array.prototype.filter applied to string primitive
---*/

        function callbackfn(val, idx, obj) {
            return obj instanceof String;
        }

        var newArr = Array.prototype.filter.call("abc", callbackfn);

assert.sameValue(newArr[0], "a", 'newArr[0]');
