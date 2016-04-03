// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-4-12
description: Array.prototype.reduceRight - 'callbackfn' is a function
---*/

        var initialValue = 0;
        function callbackfn(accum, val, idx, obj) {
            accum += val;
            return accum;
        }

assert.sameValue([11, 9].reduceRight(callbackfn, initialValue), 20, '[11, 9].reduceRight(callbackfn, initialValue)');
