// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-9-c-i-23
description: >
    Array.prototype.reduceRight - This object is an global object
    which contains index property
includes: [fnGlobalObject.js]
---*/

        var testResult = false;
        function callbackfn(prevVal, curVal, idx, obj) {
            if (idx === 1) {
                testResult = (curVal === 1);
            }
        }

            var oldLen = fnGlobalObject().length;
            fnGlobalObject()[0] = 0;
            fnGlobalObject()[1] = 1;
            fnGlobalObject()[2] = 2;
            fnGlobalObject().length = 3;

            Array.prototype.reduceRight.call(fnGlobalObject(), callbackfn, "initialValue");

assert(testResult, 'testResult !== true');
