// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-606
description: >
    ES5 Attributes - all attributes in Object.preventExtensions are
    correct
---*/

        var desc = Object.getOwnPropertyDescriptor(Object, "preventExtensions");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);

        var temp = Object.preventExtensions;

            Object.preventExtensions = "2010";

            var isWritable = (Object.preventExtensions === "2010");

            var isEnumerable = false;

            for (var prop in Object) {
                if (prop === "preventExtensions") {
                    isEnumerable = true;
                }
            }
        
            delete Object.preventExtensions;

            var isConfigurable = !Object.hasOwnProperty("preventExtensions");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
