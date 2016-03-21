// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-2-15
description: Array.prototype.reduce - 'length' is property of the global object
includes: [fnGlobalObject.js]
---*/

        function callbackfn(prevVal, curVal, idx, obj) {
            return (obj.length === 2);
        }

            var oldLen = fnGlobalObject().length;
            fnGlobalObject()[0] = 12;
            fnGlobalObject()[1] = 11;
            fnGlobalObject()[2] = 9;
            fnGlobalObject().length = 2;

assert.sameValue(Array.prototype.reduce.call(fnGlobalObject(), callbackfn, 1), true, 'Array.prototype.reduce.call(fnGlobalObject(), callbackfn, 1)');
