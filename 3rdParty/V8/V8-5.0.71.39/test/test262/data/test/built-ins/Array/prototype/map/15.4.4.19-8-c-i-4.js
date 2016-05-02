// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-8-c-i-4
description: >
    Array.prototype.map - element to be retrieved is own data property
    that overrides an inherited data property on an Array
---*/

        var kValue = "abc";

        function callbackfn(val, idx, obj) {
            if (idx === 0) {
                return val === kValue;
            }
            return false;
        }

            Array.prototype[0] = 11;

            var testResult = [kValue].map(callbackfn);

assert.sameValue(testResult[0], true, 'testResult[0]');
