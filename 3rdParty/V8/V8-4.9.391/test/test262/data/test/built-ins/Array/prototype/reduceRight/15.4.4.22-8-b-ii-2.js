// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-8-b-ii-2
description: >
    Array.prototype.reduceRight - deleted properties in step 2 are
    visible here
---*/

        var obj = { 2: "accumulator", 3: "another" };

        Object.defineProperty(obj, "length", {
            get: function () {
                delete obj[2];
                return 5;
            },
            configurable: true
        });

assert.notSameValue(Array.prototype.reduceRight.call(obj, function () { }), "accumulator", 'Array.prototype.reduceRight.call(obj, function () { })');
