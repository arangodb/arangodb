// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-598
description: >
    ES5 Attributes - all attributes in Object.getPrototypeOf are
    correct
---*/

        var desc = Object.getOwnPropertyDescriptor(Object, "getPrototypeOf");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);

        var temp = Object.getPrototypeOf;

            Object.getPrototypeOf = "2010";

            var isWritable = (Object.getPrototypeOf === "2010");

            var isEnumerable = false;

            for (var prop in Object) {
                if (prop === "getPrototypeOf") {
                    isEnumerable = true;
                }
            }
        
            delete Object.getPrototypeOf;

            var isConfigurable = !Object.hasOwnProperty("getPrototypeOf");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
