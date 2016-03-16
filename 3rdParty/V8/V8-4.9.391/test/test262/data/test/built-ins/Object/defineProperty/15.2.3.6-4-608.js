// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-608
description: ES5 Attributes - all attributes in Object.isFrozen are correct
---*/

        var desc = Object.getOwnPropertyDescriptor(Object, "isFrozen");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);

        var temp = Object.isFrozen;

            Object.isFrozen = "2010";

            var isWritable = (Object.isFrozen === "2010");

            var isEnumerable = false;

            for (var prop in Object) {
                if (prop === "isFrozen") {
                    isEnumerable = true;
                }
            }
        
            delete Object.isFrozen;

            var isConfigurable = !Object.hasOwnProperty("isFrozen");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
