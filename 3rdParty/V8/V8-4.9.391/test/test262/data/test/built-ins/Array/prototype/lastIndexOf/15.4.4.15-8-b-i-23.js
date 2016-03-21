// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.15-8-b-i-23
description: Array.prototype.lastIndexOf - This object is the global object
includes: [fnGlobalObject.js]
---*/

        var targetObj = {};

            var oldLen = fnGlobalObject().length;
            fnGlobalObject()[0] = targetObj;
            fnGlobalObject()[100] = "100";
            fnGlobalObject()[200] = "200";
            fnGlobalObject().length = 200;

assert.sameValue(Array.prototype.lastIndexOf.call(fnGlobalObject(), targetObj), 0, 'Array.prototype.lastIndexOf.call(fnGlobalObject(), targetObj)');
assert.sameValue(Array.prototype.lastIndexOf.call(fnGlobalObject(), "100"), 100, 'Array.prototype.lastIndexOf.call(fnGlobalObject(), "100")');
assert.sameValue(Array.prototype.lastIndexOf.call(fnGlobalObject(), "200"), -1, 'Array.prototype.lastIndexOf.call(fnGlobalObject(), "200")');
