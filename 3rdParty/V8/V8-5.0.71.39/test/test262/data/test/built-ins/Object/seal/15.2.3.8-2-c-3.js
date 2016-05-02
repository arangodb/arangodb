// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.8-2-c-3
description: Object.seal - 'O' is a String object
---*/

        var strObj = new String("a");
        var preCheck = Object.isExtensible(strObj);
        Object.seal(strObj);

assert(preCheck, 'preCheck !== true');
assert(Object.isSealed(strObj), 'Object.isSealed(strObj) !== true');
