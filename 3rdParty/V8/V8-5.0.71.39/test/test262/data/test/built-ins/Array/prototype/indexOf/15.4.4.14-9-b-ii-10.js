// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-9-b-ii-10
description: >
    Array.prototype.indexOf - both array element and search element
    are Boolean type, and they have same value
---*/

assert.sameValue([false, true].indexOf(true), 1, '[false, true].indexOf(true)');
