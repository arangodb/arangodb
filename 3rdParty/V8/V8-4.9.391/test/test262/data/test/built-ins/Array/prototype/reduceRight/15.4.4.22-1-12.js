// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-1-12
description: Array.prototype.reduceRight applied to RegExp object
---*/

        var obj = new RegExp();
        obj.length = 1;
        obj[0] = 1;
        var accessed = false;

        function callbackfn(prevVal, curVal, idx, o) {
            accessed = true;
            return o instanceof RegExp;
        }

assert(Array.prototype.reduceRight.call(obj, callbackfn, 1), 'Array.prototype.reduceRight.call(obj, callbackfn, 1) !== true');
assert(accessed, 'accessed !== true');
