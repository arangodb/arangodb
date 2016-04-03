// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.4.1-4-a-3-s
description: >
    TypeError isn't thrown when deleting configurable data property
---*/

        var obj = {};
        Object.defineProperty(obj, "prop", {
            value: "abc",
            configurable: true
        });

        delete obj.prop;

assert.sameValue(obj.hasOwnProperty("prop"), false, 'obj.hasOwnProperty("prop")');
