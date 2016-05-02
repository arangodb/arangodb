// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-615
description: ES5 Attributes - all attributes in Array.prototype.some are correct
---*/

        var desc = Object.getOwnPropertyDescriptor(Array.prototype, "some");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);

        var temp = Array.prototype.some;

            Array.prototype.some = "2010";

            var isWritable = (Array.prototype.some === "2010");

            var isEnumerable = false;

            for (var prop in Array.prototype) {
                if (prop === "some") {
                    isEnumerable = true;
                }
            }

            delete Array.prototype.some;

            var isConfigurable = !Array.prototype.hasOwnProperty("some");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
