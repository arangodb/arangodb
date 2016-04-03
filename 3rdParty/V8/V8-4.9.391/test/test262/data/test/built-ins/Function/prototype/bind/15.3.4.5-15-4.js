// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.3.4.5-15-4
description: >
    Function.prototype.bind - The [[Enumerable]] attribute of length
    property in F set as false
---*/

        var canEnumerable = false;
        var hasProperty = false;
        function foo() { }
        var obj = foo.bind({});
        hasProperty = obj.hasOwnProperty("length");
        for (var prop in obj) {
            if (prop === "length") {
                canEnumerable = true;
            }
        }

assert(hasProperty, 'hasProperty !== true');
assert.sameValue(canEnumerable, false, 'canEnumerable');
