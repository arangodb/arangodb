// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.8-2-c-6
description: Object.seal - 'O' is a Date object
---*/

        var dateObj = new Date();
        var preCheck = Object.isExtensible(dateObj);
        Object.seal(dateObj);

assert(preCheck, 'preCheck !== true');
assert(Object.isSealed(dateObj), 'Object.isSealed(dateObj) !== true');
