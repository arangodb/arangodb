// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-600
description: >
    ES5 Attributes - all attributes in Object.getOwnPropertyNames are
    correct
---*/

        var desc = Object.getOwnPropertyDescriptor(Object, "getOwnPropertyNames");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);

        var temp = Object.getOwnPropertyNames;

            Object.getOwnPropertyNames = "2010";

            var isWritable = (Object.getOwnPropertyNames === "2010");

            var isEnumerable = false;

            for (var prop in Object) {
                if (prop === "getOwnPropertyNames") {
                    isEnumerable = true;
                }
            }
        
            delete Object.getOwnPropertyNames;

            var isConfigurable = !Object.hasOwnProperty("getOwnPropertyNames");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
