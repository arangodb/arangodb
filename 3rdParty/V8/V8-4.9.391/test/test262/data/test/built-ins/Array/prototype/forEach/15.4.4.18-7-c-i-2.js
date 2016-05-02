// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-7-c-i-2
description: >
    Array.prototype.forEach - element to be retrieved is own data
    property on an Array
---*/

        var testResult = false;

        function callbackfn(val, idx, obj) {
            if (idx === 0) {
                testResult = (val === 11);
            }
        }

        [11].forEach(callbackfn);

assert(testResult, 'testResult !== true');
