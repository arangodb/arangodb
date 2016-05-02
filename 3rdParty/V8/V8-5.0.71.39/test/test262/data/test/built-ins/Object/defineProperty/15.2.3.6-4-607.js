// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-607
description: ES5 Attributes - all attributes in Object.isSealed are correct
---*/

        var desc = Object.getOwnPropertyDescriptor(Object, "isSealed");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);

        var temp = Object.isSealed;

            Object.isSealed = "2010";

            var isWritable = (Object.isSealed === "2010");

            var isEnumerable = false;

            for (var prop in Object) {
                if (prop === "isSealed") {
                    isEnumerable = true;
                }
            }
        
            delete Object.isSealed;

            var isConfigurable = !Object.hasOwnProperty("isSealed");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
