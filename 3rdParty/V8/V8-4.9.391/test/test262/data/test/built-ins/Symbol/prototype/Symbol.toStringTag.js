// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 19.4.3.5
description: >
    `Symbol.toStringTag` property descriptor
info: >
    The initial value of the @@toStringTag property is the String value
    "Symbol".

    This property has the attributes { [[Writable]]: false, [[Enumerable]]:
    false, [[Configurable]]: true }.
includes: [propertyHelper.js]
features: [Symbol.toStringTag]
---*/

assert.sameValue(Symbol.prototype[Symbol.toStringTag], 'Symbol');

verifyNotEnumerable(Symbol.prototype, Symbol.toStringTag);
verifyNotWritable(Symbol.prototype, Symbol.toStringTag);
verifyConfigurable(Symbol.prototype, Symbol.toStringTag);
