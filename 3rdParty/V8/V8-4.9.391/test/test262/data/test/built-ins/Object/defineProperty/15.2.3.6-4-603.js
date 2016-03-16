// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-603
description: >
    ES5 Attributes - all attributes in Object.defineProperties are
    correct
---*/

        var desc = Object.getOwnPropertyDescriptor(Object, "defineProperties");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);

        var temp = Object.defineProperties;

            Object.defineProperties = "2010";

            var isWritable = (Object.defineProperties === "2010");

            var isEnumerable = false;

            for (var prop in Object) {
                if (prop === "defineProperties") {
                    isEnumerable = true;
                }
            }
        
            delete Object.defineProperties;

            var isConfigurable = !Object.hasOwnProperty("defineProperties");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
