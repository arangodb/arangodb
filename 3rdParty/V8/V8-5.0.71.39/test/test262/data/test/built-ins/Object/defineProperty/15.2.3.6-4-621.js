// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-621
description: >
    ES5 Attributes - all attributes in String.prototype.trim are
    correct
---*/

        var desc = Object.getOwnPropertyDescriptor(String.prototype, "trim");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);

        var temp = String.prototype.trim;

            String.prototype.trim = "2010";

            var isWritable = (String.prototype.trim === "2010");

            var isEnumerable = false;

            for (var prop in String.prototype) {
                if (prop === "trim") {
                    isEnumerable = true;
                }
            }

            delete String.prototype.trim;

            var isConfigurable = !String.prototype.hasOwnProperty("trim");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
