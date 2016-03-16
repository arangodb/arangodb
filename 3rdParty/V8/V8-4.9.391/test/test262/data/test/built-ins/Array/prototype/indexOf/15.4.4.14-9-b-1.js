// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-9-b-1
description: Array.prototype.indexOf - non-existent property wouldn't be called
---*/

assert.sameValue([0, , 2].indexOf(undefined), -1, '[0, , 2].indexOf(undefined)');
