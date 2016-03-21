// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-7-10
description: Array.prototype.reduce - 'initialValue' is present
---*/

        var str = "initialValue is present";

assert.sameValue([].reduce(function () { }, str), str, '[].reduce(function () { }, str)');
