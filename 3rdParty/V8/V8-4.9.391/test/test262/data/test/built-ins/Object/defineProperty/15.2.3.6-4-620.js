// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-620
description: >
    ES5 Attributes - all attributes in Array.prototype.reduceRight are
    correct
---*/

        var desc = Object.getOwnPropertyDescriptor(Array.prototype, "reduceRight");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);

        var temp = Array.prototype.reduceRight;

            Array.prototype.reduceRight = "2010";

            var isWritable = (Array.prototype.reduceRight === "2010");

            var isEnumerable = false;

            for (var prop in Array.prototype) {
                if (prop === "reduceRight") {
                    isEnumerable = true;
                }
            }

            delete Array.prototype.reduceRight;

            var isConfigurable = !Array.prototype.hasOwnProperty("reduceRight");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
