// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.3.4.45
description: Date.prototype[Symbol.toPrimitive] property descriptor
info: >
    This property has the attributes { [[Writable]]: false, [[Enumerable]]:
    false, [[Configurable]]: true }.
includes: [propertyHelper.js]
features: [Symbol.toPrimitive]
---*/

verifyNotEnumerable(Date.prototype, Symbol.toPrimitive);
verifyNotWritable(Date.prototype, Symbol.toPrimitive);
verifyConfigurable(Date.prototype, Symbol.toPrimitive);
