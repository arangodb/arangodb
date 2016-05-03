// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-1-1
description: Array.prototype.reduceRight applied to undefined throws a TypeError
---*/


assert.throws(TypeError, function() {
            Array.prototype.reduceRight.call(undefined);
});
