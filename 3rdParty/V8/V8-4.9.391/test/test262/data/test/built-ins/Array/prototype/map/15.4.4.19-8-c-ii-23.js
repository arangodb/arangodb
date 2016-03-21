// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-8-c-ii-23
description: >
    Array.prototype.map - callbackfn called with correct parameters
    (this object O is correct)
---*/

        var obj = { 0: 11, length: 2 };

        function callbackfn(val, idx, o) {
            return obj === o;
        }

        var testResult = Array.prototype.map.call(obj, callbackfn);

assert.sameValue(testResult[0], true, 'testResult[0]');
