// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-9-c-i-2
description: >
    Array.prototype.reduce - element to be retrieved is own data
    property on an Array
---*/

        var testResult = false;
        var initialValue = 0;
        function callbackfn(prevVal, curVal, idx, obj) {
            if (idx === 1) {
                testResult = (curVal === 1);
            }
        }

        var arr = [0, 1];
        arr.reduce(callbackfn, initialValue);

assert(testResult, 'testResult !== true');
