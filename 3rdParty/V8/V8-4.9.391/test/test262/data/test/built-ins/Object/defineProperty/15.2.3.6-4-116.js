// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-116
description: >
    Object.defineProperty - 'O' is an Array, test the length property
    of 'O' is own data property (15.4.5.1 step 1)
---*/

        var arrObj = [0, 1];
        Object.defineProperty(arrObj, "1", {
            value: 1,
            configurable: false
        });

assert.throws(TypeError, function() {
    Object.defineProperty(arrObj, "length", { value: 1 });
});

var desc = Object.getOwnPropertyDescriptor(arrObj, "length");

assert(Object.hasOwnProperty.call(arrObj, "length"), 'Object.hasOwnProperty.call(arrObj, "length")');
assert.sameValue(desc.value, 2, 'desc.value');
assert.sameValue(desc.writable, true, 'desc.writable');
assert.sameValue(desc.configurable, false, 'desc.configurable');
assert.sameValue(desc.enumerable, false, 'desc.enumerable');
