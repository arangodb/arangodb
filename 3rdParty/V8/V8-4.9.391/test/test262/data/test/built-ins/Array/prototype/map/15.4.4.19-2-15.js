// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-2-15
description: >
    Array.prototype.map - when 'length' is property of the global
    object
includes: [fnGlobalObject.js]
---*/

        function callbackfn(val, idx, obj) {
            return val > 10;
        }

            var oldLen = fnGlobalObject().length;
            fnGlobalObject()[0] = 12;
            fnGlobalObject()[1] = 11;
            fnGlobalObject()[2] = 9;
            fnGlobalObject().length = 2;
            var testResult = Array.prototype.map.call(fnGlobalObject(), callbackfn);

assert.sameValue(testResult.length, 2, 'testResult.length');
