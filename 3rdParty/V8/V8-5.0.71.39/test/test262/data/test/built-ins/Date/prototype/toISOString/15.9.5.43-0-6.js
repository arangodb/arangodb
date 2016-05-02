// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.9.5.43-0-6
description: >
    Date.prototype.toISOString - TypeError is thrown when this is any
    other objects instead of Date object
---*/


assert.throws(TypeError, function() {
            Date.prototype.toISOString.call([]);
});
