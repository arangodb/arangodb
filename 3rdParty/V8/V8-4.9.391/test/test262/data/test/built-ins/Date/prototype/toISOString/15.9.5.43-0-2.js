// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.9.5.43-0-2
description: >
    Date.prototype.toISOString must exist as a function taking 0
    parameters
---*/

assert.sameValue(Date.prototype.toISOString.length, 0, 'Date.prototype.toISOString.length');
