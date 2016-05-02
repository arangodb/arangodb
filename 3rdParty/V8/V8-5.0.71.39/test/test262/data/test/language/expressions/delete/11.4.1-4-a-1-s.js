// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.4.1-4-a-1-s
description: >
    Strict Mode - TypeError is thrown when deleting non-configurable
    data property
flags: [onlyStrict]
---*/

        var obj = {};
        Object.defineProperty(obj, "prop", {
            value: "abc",
            configurable: false
        });
assert.throws(TypeError, function() {
            delete obj.prop;
});
assert.sameValue(obj.prop, "abc", 'obj.prop');
