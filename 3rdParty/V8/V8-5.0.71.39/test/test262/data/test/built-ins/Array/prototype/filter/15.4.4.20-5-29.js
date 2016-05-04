// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.20-5-29
description: Array.prototype.filter - returns an array whose length is 0
---*/

        var newArr = [11].filter(function () { });

assert.sameValue(newArr.length, 0, 'newArr.length');
