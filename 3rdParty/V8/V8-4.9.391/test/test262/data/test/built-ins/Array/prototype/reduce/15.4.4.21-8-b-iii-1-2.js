// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-8-b-iii-1-2
description: >
    Array.prototype.reduce - element to be retrieved is own data
    property on an Array
---*/

        var testResult = false;
        function callbackfn(prevVal, curVal, idx, obj) {
            if (idx === 1) {
                testResult = (prevVal === 0);
            }
        }

        var arr = [0, 1, 2];
        arr.reduce(callbackfn);

assert(testResult, 'testResult !== true');
