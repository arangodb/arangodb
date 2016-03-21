// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-9-c-ii-11
description: >
    Array.prototype.reduceRight - callbackfn is called with 2 formal
    parameter
---*/

        var testResult = false;

        function callbackfn(prevVal, curVal) {
            if (prevVal === 100) {
                testResult = true;
            }
            return curVal > 10;
        }

assert.sameValue([11].reduceRight(callbackfn, 100), true, '[11].reduceRight(callbackfn, 100)');
assert(testResult, 'testResult !== true');
