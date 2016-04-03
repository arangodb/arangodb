// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-601
description: ES5 Attributes - all attributes in Object.create are correct
---*/

        var desc = Object.getOwnPropertyDescriptor(Object, "create");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);

        var temp = Object.create;

            Object.create = "2010";

            var isWritable = (Object.create === "2010");

            var isEnumerable = false;

            for (var prop in Object) {
                if (prop === "create") {
                    isEnumerable = true;
                }
            }
        
            delete Object.create;

            var isConfigurable = !Object.hasOwnProperty("create");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
