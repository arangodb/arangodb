// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-1-17
description: Array.prototype.indexOf applied to the global object
includes: [fnGlobalObject.js]
---*/

            var oldLen = fnGlobalObject().length;
            fnGlobalObject()[1] = true;
            fnGlobalObject().length = 2;

assert.sameValue(Array.prototype.indexOf.call(fnGlobalObject(), true), 1, 'Array.prototype.indexOf.call(fnGlobalObject(), true)');
