// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.9.5.43-0-7
description: >
    Date.prototype.toISOString - TypeError is thrown when this is any
    primitive values
---*/


assert.throws(TypeError, function() {
            Date.prototype.toISOString.call(15);
});
