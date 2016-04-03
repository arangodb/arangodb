// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.13-2-29
description: Object.isExtensible returns true for the global object
includes: [fnGlobalObject.js]
---*/

assert(Object.isExtensible(fnGlobalObject()), 'Object.isExtensible(fnGlobalObject()) !== true');
