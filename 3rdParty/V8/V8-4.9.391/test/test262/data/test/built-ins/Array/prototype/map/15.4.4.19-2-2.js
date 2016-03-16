// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-2-2
description: >
    Array.prototype.map - when 'length' is own data property on an
    Array
---*/

        function callbackfn(val, idx, obj) {
            return val > 10;
        }

        var testResult = [12, 11].map(callbackfn);

assert.sameValue(testResult.length, 2, 'testResult.length');
