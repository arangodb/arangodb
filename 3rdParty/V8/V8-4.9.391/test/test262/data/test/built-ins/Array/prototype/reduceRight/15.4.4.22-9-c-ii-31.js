// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-9-c-ii-31
description: >
    Array.prototype.reduceRight - Date Object can be used as
    accumulator
---*/

        var accessed = false;
        var objDate = new Date();
        function callbackfn(prevVal, curVal, idx, obj) {
            accessed = true;
            return prevVal === objDate;
        }

        var obj = { 0: 11, length: 1 };

assert.sameValue(Array.prototype.reduceRight.call(obj, callbackfn, objDate), true, 'Array.prototype.reduceRight.call(obj, callbackfn, objDate)');
assert(accessed, 'accessed !== true');
