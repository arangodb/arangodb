// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-9-c-ii-8
description: >
    Array.prototype.reduceRight - element changed by callbackfn on
    previous iterations is observed
---*/

        var accessed = false;
        var obj = { 0: 11, 1: 12, length: 2 };
        function callbackfn(prevVal, curVal, idx, obj) {
            accessed = true;
            if (idx === 1) {
                obj[idx - 1] = 8;
            }
            return curVal > 10;
        }

assert.sameValue(Array.prototype.reduceRight.call(obj, callbackfn, 1), false, 'Array.prototype.reduceRight.call(obj, callbackfn, 1)');
assert(accessed, 'accessed !== true');
