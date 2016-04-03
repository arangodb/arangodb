// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.15-2-15
description: >
    Array.prototype.lastIndexOf - 'length' is property of the global
    object
includes: [fnGlobalObject.js]
---*/

        var targetObj = {};

            var oldLen = fnGlobalObject().length;
            fnGlobalObject().length = 2;

            fnGlobalObject()[1] = targetObj;

assert.sameValue(Array.prototype.lastIndexOf.call(fnGlobalObject(), targetObj), 1);

            fnGlobalObject()[1] = {};
            fnGlobalObject()[2] = targetObj;

assert.sameValue(Array.prototype.lastIndexOf.call(fnGlobalObject(), targetObj), -1);
