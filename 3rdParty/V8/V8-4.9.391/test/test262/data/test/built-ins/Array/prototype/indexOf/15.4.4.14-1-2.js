// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-1-2
description: Array.prototype.indexOf applied to null throws a TypeError
---*/


assert.throws(TypeError, function() {
     Array.prototype.indexOf.call(null);
});
