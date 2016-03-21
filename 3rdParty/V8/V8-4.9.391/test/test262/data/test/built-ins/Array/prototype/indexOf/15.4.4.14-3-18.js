// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-3-18
description: >
    Array.prototype.indexOf - value of 'length' is a string that can't
    convert to a number
---*/

        var obj = { 0: true, 100: true, length: "one" };

assert.sameValue(Array.prototype.indexOf.call(obj, true), -1, 'Array.prototype.indexOf.call(obj, true)');
