// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-1-1
description: Array.prototype.some applied to undefined throws a TypeError
---*/


assert.throws(TypeError, function() {
            Array.prototype.some.call(undefined);
});
