// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-2-15
description: >
    Array.prototype.reduceRight - 'length' is property of the global
    object
includes: [fnGlobalObject.js]
---*/

        var accessed = false;

        function callbackfn(prevVal, curVal, idx, obj) {
            accessed = true;
            return obj.length === fnGlobalObject().length;
        }

            var oldLen = fnGlobalObject().length;
            fnGlobalObject()[0] = 12;
            fnGlobalObject()[1] = 11;
            fnGlobalObject()[2] = 9;
            fnGlobalObject().length = 2;

assert(Array.prototype.reduceRight.call(fnGlobalObject(), callbackfn, 111), 'Array.prototype.reduceRight.call(fnGlobalObject(), callbackfn, 111) !== true');
assert(accessed, 'accessed !== true');
