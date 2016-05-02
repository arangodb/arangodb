// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-9-c-ii-16
description: >
    Array.prototype.reduceRight - non-indexed properties are not
    called on an Array-like object
---*/

        var testResult = false;

        function callbackfn(prevVal, curVal, idx, obj) {
            if (prevVal === 8 || curVal === 8) {
                testResult = true;
            }
        }

        var obj = { 0: 11, 10: 12, non_index_property: 8, length: 20 };
        Array.prototype.reduceRight.call(obj, callbackfn, "initialValue");

assert.sameValue(testResult, false, 'testResult');
