// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-9-c-ii-18
description: >
    Array.prototype.reduceRight - 'accumulator' used for first
    iteration is the value of 'initialValue' when it is present on an
    Array
---*/

        var arr = [11, 12];
        var testResult = false;
        var initVal = 6.99;

        function callbackfn(prevVal, curVal, idx, obj) {
            if (idx === 1) {
                testResult = (prevVal === initVal);
            }
            return curVal;
        }

        arr.reduceRight(callbackfn, initVal);

assert(testResult, 'testResult !== true');
