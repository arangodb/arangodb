// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-8-c-ii-16
description: >
    Array.prototype.map - 'this' object when T is not an object (T is
    a boolean primitive)
---*/

        function callbackfn(val, idx, obj) {
            return this.valueOf() === false;
        }

        var obj = { 0: 11, length: 2 };

        var testResult = Array.prototype.map.call(obj, callbackfn, false);

assert.sameValue(testResult[0], true, 'testResult[0]');
