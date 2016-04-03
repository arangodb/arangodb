// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-1-2
description: Array.prototype.reduce applied to null
---*/


assert.throws(TypeError, function() {
            Array.prototype.reduce.call(null);
});
