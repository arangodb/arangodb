// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-3-1
description: Array.prototype.reduceRight - value of 'length' is undefined
---*/

        var accessed = false;

        function callbackfn(prevVal, curVal, idx, obj) {
            accessed = true;
        }

        var obj = { 0: 9, length: undefined };

assert.sameValue(Array.prototype.reduceRight.call(obj, callbackfn, 1), 1, 'Array.prototype.reduceRight.call(obj, callbackfn, 1)');
assert.sameValue(accessed, false, 'accessed');
