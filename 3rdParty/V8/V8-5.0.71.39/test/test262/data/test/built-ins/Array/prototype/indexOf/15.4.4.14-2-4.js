// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-2-4
description: >
    Array.prototype.indexOf - 'length' is own data property that
    overrides an inherited data property on an Array
---*/

        var targetObj = {};
        var arrProtoLen;

            arrProtoLen = Array.prototype.length;
            Array.prototype.length = 0;

assert.sameValue([0, targetObj].indexOf(targetObj), 1, '[0, targetObj].indexOf(targetObj)');
