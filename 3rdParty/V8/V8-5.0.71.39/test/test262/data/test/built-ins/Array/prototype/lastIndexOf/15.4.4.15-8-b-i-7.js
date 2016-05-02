// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.15-8-b-i-7
description: >
    Array.prototype.lastIndexOf - element to be retrieved is inherited
    data property on an Array
---*/

            Array.prototype[0] = true;
            Array.prototype[1] = false;
            Array.prototype[2] = "true";

assert.sameValue([, , , ].lastIndexOf(true), 0, '[, , , ].lastIndexOf(true)');
assert.sameValue([, , , ].lastIndexOf(false), 1, '[, , , ].lastIndexOf(false)');
assert.sameValue([, , , ].lastIndexOf("true"), 2, '[, , , ].lastIndexOf("true")');
