// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-602
description: >
    ES5 Attributes - all attributes in Object.defineProperty are
    correct
---*/

        var desc = Object.getOwnPropertyDescriptor(Object, "defineProperty");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);
        var temp = Object.defineProperty;

            Object.defineProperty = "2010";

            var isWritable = (Object.defineProperty === "2010");

            var isEnumerable = false;

            for (var prop in Object) {
                if (prop === "defineProperty") {
                    isEnumerable = true;
                }
            }

            delete Object.defineProperty;

            var isConfigurable = !Object.hasOwnProperty("defineProperty");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
