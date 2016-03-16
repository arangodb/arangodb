// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.8-3-1
description: Object.seal - returned object is not extensible
---*/

        var obj = {};
        var preCheck = Object.isExtensible(obj);
        Object.seal(obj);

assert(preCheck, 'preCheck !== true');
assert.sameValue(Object.isExtensible(obj), false, 'Object.isExtensible(obj)');
