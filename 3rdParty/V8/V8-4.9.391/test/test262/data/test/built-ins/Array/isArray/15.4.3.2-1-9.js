// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.3.2-1-9
description: Array.isArray applied to Date object
---*/

assert.sameValue(Array.isArray(new Date()), false, 'Array.isArray(new Date())');
