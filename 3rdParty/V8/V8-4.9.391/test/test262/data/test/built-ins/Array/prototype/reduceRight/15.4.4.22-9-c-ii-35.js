// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-9-c-ii-35
description: >
    Array.prototype.reduceRight - the Arguments object can be used as
    accumulator
---*/

        var accessed = false;
        var arg;

        function callbackfn(prevVal, curVal, idx, obj) {
            accessed = true;
            return prevVal === arg;
        }

        var obj = { 0: 11, length: 1 };

        (function fun() {
            arg = arguments;
        }(10, 11, 12, 13));

assert.sameValue(Array.prototype.reduceRight.call(obj, callbackfn, arg), true, 'Array.prototype.reduceRight.call(obj, callbackfn, arg)');
assert(accessed, 'accessed !== true');
