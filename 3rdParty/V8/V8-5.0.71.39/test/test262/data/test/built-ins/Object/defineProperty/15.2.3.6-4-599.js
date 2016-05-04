// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-599
description: >
    ES5 Attributes - all attributes in Object.getOwnPropertyDescriptor
    are correct
---*/

        var desc = Object.getOwnPropertyDescriptor(Object, "getOwnPropertyDescriptor");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);

        var temp = Object.getOwnPropertyDescriptor;

            Object.getOwnPropertyDescriptor = "2010";

            var isWritable = (Object.getOwnPropertyDescriptor === "2010");

            var isEnumerable = false;

            for (var prop in Object) {
                if (prop === "getOwnPropertyDescriptor") {
                    isEnumerable = true;
                }
            }

            delete Object.getOwnPropertyDescriptor;

            var isConfigurable = !Object.hasOwnProperty("getOwnPropertyDescriptor");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
