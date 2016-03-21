// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.8-2-c-7
description: Object.seal - 'O' is a RegExp object
---*/

        var regObj = new RegExp();
        var preCheck = Object.isExtensible(regObj);
        Object.seal(regObj);

assert(preCheck, 'preCheck !== true');
assert(Object.isSealed(regObj), 'Object.isSealed(regObj) !== true');
