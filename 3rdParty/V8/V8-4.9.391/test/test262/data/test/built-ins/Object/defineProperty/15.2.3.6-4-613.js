// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-613
description: ES5 Attributes - all attributes in Object.lastIndexOf are correct
---*/

        var desc = Object.getOwnPropertyDescriptor(Array.prototype, "lastIndexOf");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);

        var temp = Array.prototype.lastIndexOf;

            Array.prototype.lastIndexOf = "2010";

            var isWritable = (Array.prototype.lastIndexOf === "2010");

            var isEnumerable = false;

            for (var prop in Array.prototype) {
                if (prop === "lastIndexOf") {
                    isEnumerable = true;
                }
            }

            delete Array.prototype.lastIndexOf;

            var isConfigurable = !Array.prototype.hasOwnProperty("lastIndexOf");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
