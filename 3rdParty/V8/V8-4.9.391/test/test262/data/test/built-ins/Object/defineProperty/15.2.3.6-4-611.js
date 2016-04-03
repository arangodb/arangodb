// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-611
description: >
    ES5 Attributes - all attributes in Function.prototype.bind are
    correct
---*/

        var desc = Object.getOwnPropertyDescriptor(Function.prototype, "bind");

        var propertyAreCorrect = (desc.writable === true && desc.enumerable === false && desc.configurable === true);

        var temp = Function.prototype.bind;

            Function.prototype.bind = "2010";

            var isWritable = (Function.prototype.bind === "2010");

            var isEnumerable = false;

            for (var prop in Function.prototype) {
                if (prop === "bind") {
                    isEnumerable = true;
                }
            }
        
            delete Function.prototype.bind;

            var isConfigurable = !Function.prototype.hasOwnProperty("bind");

assert(propertyAreCorrect, 'propertyAreCorrect !== true');
assert(isWritable, 'isWritable !== true');
assert.sameValue(isEnumerable, false, 'isEnumerable');
assert(isConfigurable, 'isConfigurable !== true');
