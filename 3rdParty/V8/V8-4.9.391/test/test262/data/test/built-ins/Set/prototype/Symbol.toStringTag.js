// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.12
description: >
    `Symbol.toStringTag` property descriptor
info: >
    The initial value of the @@toStringTag property is the String value
    "Set".

    This property has the attributes { [[Writable]]: false, [[Enumerable]]:
    false, [[Configurable]]: true }.
includes: [propertyHelper.js]
features: [Symbol.toStringTag]
---*/

assert.sameValue(Set.prototype[Symbol.toStringTag], 'Set');

verifyNotEnumerable(Set.prototype, Symbol.toStringTag);
verifyNotWritable(Set.prototype, Symbol.toStringTag);
verifyConfigurable(Set.prototype, Symbol.toStringTag);
