// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 24.2.4.21
description: >
    `Symbol.toStringTag` property descriptor
info: >
    The initial value of the @@toStringTag property is the String value
    "DataView".

    This property has the attributes { [[Writable]]: false, [[Enumerable]]:
    false, [[Configurable]]: true }.
includes: [propertyHelper.js]
features: [Symbol.toStringTag]
---*/

assert.sameValue(DataView.prototype[Symbol.toStringTag], 'DataView');

verifyNotEnumerable(DataView.prototype, Symbol.toStringTag);
verifyNotWritable(DataView.prototype, Symbol.toStringTag);
verifyConfigurable(DataView.prototype, Symbol.toStringTag);
