// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.3.2-1-15
description: Array.isArray applied to the global object
includes: [fnGlobalObject.js]
---*/

assert.sameValue(Array.isArray(fnGlobalObject()), false, 'Array.isArray(fnGlobalObject())');
