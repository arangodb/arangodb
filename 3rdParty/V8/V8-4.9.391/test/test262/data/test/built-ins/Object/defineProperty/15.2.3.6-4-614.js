// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-614
description: >
    ES5 Attributes - all attributes in Array.prototype.every are
    correct
---*/

        var desc = Object.getOwnPropertyDescriptor(Array.prototype, "every");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);

        var temp = Array.prototype.every;

            Array.prototype.every = "2010";

            var isWritable = (Array.prototype.every === "2010");

            var isEnumerable = false;

            for (var prop in Array.prototype) {
                if (prop === "every") {
                    isEnumerable = true;
                }
            }

            delete Array.prototype.every;

            var isConfigurable = !Array.prototype.hasOwnProperty("every");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
