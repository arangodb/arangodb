// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-6-2
description: Array.prototype.map - the returned array is instanceof Array
---*/

        var newArr = [11].map(function () { });

assert(newArr instanceof Array, 'newArr instanceof Array !== true');
