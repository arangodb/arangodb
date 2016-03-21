// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-624
description: >
    ES5 Attributes - all attributes in Date.prototype.toJSON are
    correct
---*/

        var desc = Object.getOwnPropertyDescriptor(Date.prototype, "toJSON");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);

        var temp = Date.prototype.toJSON;

            Date.prototype.toJSON = "2010";

            var isWritable = (Date.prototype.toJSON === "2010");

            var isEnumerable = false;

            for (var prop in Date.prototype) {
                if (prop === "toJSON") {
                    isEnumerable = true;
                }
            }

            delete Date.prototype.toJSON;

            var isConfigurable = !Date.prototype.hasOwnProperty("toJSON");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
