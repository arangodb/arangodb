// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.12
description: Initial state of the Symbol.iterator property
info: >
  The initial value of the @@iterator property is the same function object as
  the initial value of the entries property.

  Per ES6 section 17, the method should exist on the Array prototype, and it
  should be writable and configurable, but not enumerable.
includes: [propertyHelper.js]
features: [Symbol.iterator]
---*/

assert.sameValue(Map.prototype[Symbol.iterator], Map.prototype.entries);
verifyNotEnumerable(Map.prototype, Symbol.iterator);
verifyWritable(Map.prototype, Symbol.iterator);
verifyConfigurable(Map.prototype, Symbol.iterator);
