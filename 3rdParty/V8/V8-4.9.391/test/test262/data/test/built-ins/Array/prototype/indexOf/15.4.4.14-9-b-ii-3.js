// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-9-b-ii-3
description: >
    Array.prototype.indexOf - both type of array element and type of
    search element are null
---*/

assert.sameValue([null].indexOf(null), 0, '[null].indexOf(null)');
