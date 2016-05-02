// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-610
description: ES5 Attributes - all attributes in Object.keys are correct
---*/

        var desc = Object.getOwnPropertyDescriptor(Object, "keys");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);

        var temp = Object.keys;

            Object.keys = "2010";

            var isWritable = (Object.keys === "2010");

            var isEnumerable = false;

            for (var prop in Object) {
                if (prop === "keys") {
                    isEnumerable = true;
                }
            }
        
            delete Object.keys;

            var isConfigurable = !Object.hasOwnProperty("keys");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
