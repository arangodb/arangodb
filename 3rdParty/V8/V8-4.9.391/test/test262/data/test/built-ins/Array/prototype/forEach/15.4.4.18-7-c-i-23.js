// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-7-c-i-23
description: >
    Array.prototype.forEach - This object is an global object which
    contains index property
includes: [fnGlobalObject.js]
---*/

        var testResult = false;

        function callbackfn(val, idx, obj) {
            if (idx === 0) {
                testResult = (val === 11);
            }
        }

            var oldLen = fnGlobalObject().length;
            fnGlobalObject()[0] = 11;
            fnGlobalObject().length = 1;

            Array.prototype.forEach.call(fnGlobalObject(), callbackfn);

assert(testResult, 'testResult !== true');
