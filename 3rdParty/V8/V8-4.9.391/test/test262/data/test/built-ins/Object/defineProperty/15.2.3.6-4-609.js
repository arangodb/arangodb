// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-609
description: ES5 Attributes - all attributes in Object.isExtensible are correct
---*/

        var desc = Object.getOwnPropertyDescriptor(Object, "isExtensible");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);

        var temp = Object.isExtensible;

            Object.isExtensible = "2010";

            var isWritable = (Object.isExtensible === "2010");

            var isEnumerable = false;

            for (var prop in Object) {
                if (prop === "isExtensible") {
                    isEnumerable = true;
                }
            }
        
            delete Object.isExtensible;

            var isConfigurable = !Object.hasOwnProperty("isExtensible");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
