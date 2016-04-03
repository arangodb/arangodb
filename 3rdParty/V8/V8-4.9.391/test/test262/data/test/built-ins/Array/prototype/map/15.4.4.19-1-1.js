// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-1-1
description: Array.prototype.map - applied to undefined
---*/


assert.throws(TypeError, function() {
            Array.prototype.map.call(undefined); // TypeError is thrown if value is undefined
});
