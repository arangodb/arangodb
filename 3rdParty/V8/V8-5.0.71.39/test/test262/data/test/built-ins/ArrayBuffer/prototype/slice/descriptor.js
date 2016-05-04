// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 24.1.4.3
description: >
  ArrayBuffer.prototype.slice has default data property attributes.
info: >
  ArrayBuffer.prototype.slice ( start, end )

  17 ECMAScript Standard Built-in Objects:
    Every other data property described in clauses 18 through 26 and in
    Annex B.2 has the attributes { [[Writable]]: true, [[Enumerable]]: false,
    [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js]
---*/

verifyNotEnumerable(ArrayBuffer.prototype, "slice");
verifyWritable(ArrayBuffer.prototype, "slice");
verifyConfigurable(ArrayBuffer.prototype, "slice");
