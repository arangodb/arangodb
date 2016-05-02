// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-7-c-i-8
description: >
    Array.prototype.forEach - element to be retrieved is inherited
    data property on an Array
---*/

        var testResult = false;

        function callbackfn(val, idx, obj) {
            if (idx === 1) {
                testResult = (val === 13);
            }
        }

            Array.prototype[1] = 13;

            [, , , ].forEach(callbackfn);

assert(testResult, 'testResult !== true');
