// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-7-2
description: >
    Array.prototype.indexOf returns correct index when 'fromIndex' is
    length of array - 1
---*/

assert.sameValue([1, 2, 3].indexOf(3, 2), 2, '[1, 2, 3].indexOf(3, 2)');
