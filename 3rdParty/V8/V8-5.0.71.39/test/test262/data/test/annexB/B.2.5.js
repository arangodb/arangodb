// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: B.2.5
description: >
    Object.getOwnPropertyDescriptor returns data desc for functions on
    built-ins (Date.prototype.setYear)
includes: [propertyHelper.js]
---*/

verifyWritable(Date.prototype, "setYear");
verifyNotEnumerable(Date.prototype, "setYear");
verifyConfigurable(Date.prototype, "setYear");
