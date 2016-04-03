// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.10.7.2-1
description: RegExp.prototype.global is a non-generic accessor property
---*/


assert.throws(TypeError, function() {
    RegExp.prototype.global;
});
