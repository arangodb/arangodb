// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.1.3.27
description: Property descriptor
info: >
    ES6 Section 17

    Every other data property described in clauses 18 through 26 and in Annex
    B.2 has the attributes { [[Writable]]: true, [[Enumerable]]: false,
    [[Configurable]]: true } unless otherwise specified.
features: [Symbol.iterator]
includes: [propertyHelper.js]
---*/

assert.sameValue(typeof String.prototype[Symbol.iterator], 'function');
verifyNotEnumerable(String.prototype, Symbol.iterator);
verifyWritable(String.prototype, Symbol.iterator);
verifyConfigurable(String.prototype, Symbol.iterator);
