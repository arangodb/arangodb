// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-9-b-i-5
description: >
    Array.prototype.indexOf - element to be retrieved is own data
    property that overrides an inherited accessor property on an Array
---*/

            Object.defineProperty(Array.prototype, "0", {
                get: function () {
                    return false;
                },
                configurable: true
            });

assert.sameValue([true].indexOf(true), 0, '[true].indexOf(true)');
