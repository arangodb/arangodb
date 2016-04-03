// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 24.1.4.2
description: >
  The `ArrayBuffer.prototype.constructor` property descriptor.
info: >
  The initial value of ArrayBuffer.prototype.constructor is the intrinsic
  object %ArrayBuffer%.

  17 ECMAScript Standard Built-in Objects:
    Every other data property described in clauses 18 through 26 and in
    Annex B.2 has the attributes { [[Writable]]: true, [[Enumerable]]: false,
    [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js]
---*/

assert.sameValue(ArrayBuffer.prototype.constructor, ArrayBuffer);

verifyNotEnumerable(ArrayBuffer.prototype, "constructor");
verifyWritable(ArrayBuffer.prototype, "constructor");
verifyConfigurable(ArrayBuffer.prototype, "constructor");
