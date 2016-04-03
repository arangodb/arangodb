// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.15-6-4
description: >
    Array.prototype.lastIndexOf returns -1 when 'fromIndex' and
    'length' are both 0
---*/

assert.sameValue([].lastIndexOf(1, 0), -1, '[].lastIndexOf(1, 0)');
