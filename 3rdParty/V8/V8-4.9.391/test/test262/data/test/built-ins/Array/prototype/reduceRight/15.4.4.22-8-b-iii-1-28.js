// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-8-b-iii-1-28
description: >
    Array.prototype.reduceRight applied to String object, which
    implements its own property get method
---*/

        var testResult = false;
        function callbackfn(prevVal, curVal, idx, obj) {
            if (idx === 1) {
                testResult = (prevVal === "2");
            }
        }

        var str = new String("012");

        Array.prototype.reduceRight.call(str, callbackfn);

assert(testResult, 'testResult !== true');
