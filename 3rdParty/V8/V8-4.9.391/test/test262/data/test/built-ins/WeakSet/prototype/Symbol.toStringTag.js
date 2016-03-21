// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.4.3.5
description: >
    `Symbol.toStringTag` property descriptor
info: >
    The initial value of the @@toStringTag property is the String value
    "WeakSet".

    This property has the attributes { [[Writable]]: false, [[Enumerable]]:
    false, [[Configurable]]: true }.
includes: [propertyHelper.js]
features: [Symbol.toStringTag]
---*/

assert.sameValue(WeakSet.prototype[Symbol.toStringTag], 'WeakSet');

verifyNotEnumerable(WeakSet.prototype, Symbol.toStringTag);
verifyNotWritable(WeakSet.prototype, Symbol.toStringTag);
verifyConfigurable(WeakSet.prototype, Symbol.toStringTag);
