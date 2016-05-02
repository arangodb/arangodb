// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.4.2-2-2
description: >
    Object.prototype.toString - '[object Null]' will be returned when
    'this' value is null
---*/

assert.sameValue(Object.prototype.toString.apply(null, []), "[object Null]", 'Object.prototype.toString.apply(null, [])');
