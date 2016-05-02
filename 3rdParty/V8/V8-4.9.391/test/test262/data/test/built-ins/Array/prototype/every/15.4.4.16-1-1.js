// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-1-1
description: Array.prototype.every applied to undefined throws a TypeError
---*/


assert.throws(TypeError, function() {
            Array.prototype.every.call(undefined); // TypeError is thrown if value is undefined
});
