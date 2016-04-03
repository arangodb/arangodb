// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: B.2.6
description: >
    Object.getOwnPropertyDescriptor returns data desc for functions on
    built-ins (Date.prototype.toGMTString)
includes: [propertyHelper.js]

---*/

verifyWritable(Date.prototype, "toGMTString");
verifyNotEnumerable(Date.prototype, "toGMTString");
verifyConfigurable(Date.prototype, "toGMTString");
