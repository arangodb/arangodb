// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: B.2.4
description: >
    Object.getOwnPropertyDescriptor returns data desc for functions on
    built-ins (Date.prototype.getYear)
includes: [propertyHelper.js]
---*/

verifyWritable(Date.prototype, "getYear");
verifyNotEnumerable(Date.prototype, "getYear");
verifyConfigurable(Date.prototype, "getYear");
