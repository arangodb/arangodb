// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.20-5-11
description: Array.prototype.filter - String Object can be used as thisArg
---*/

        var accessed = false;
        var objString = new String();

        function callbackfn(val, idx, obj) {
            accessed = true;
            return this === objString;
        }

        var newArr = [11].filter(callbackfn, objString);

assert.sameValue(newArr[0], 11, 'newArr[0]');
assert(accessed, 'accessed !== true');
