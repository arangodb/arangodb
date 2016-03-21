// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.4.3.4
description: >
  WeakSet.prototype.has property descriptor
info: >
  WeakSet.prototype.has ( value )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  typeof WeakSet.prototype.has,
  'function',
  'typeof WeakSet.prototype.has is "function"'
);

verifyNotEnumerable(WeakSet.prototype, 'has');
verifyWritable(WeakSet.prototype, 'has');
verifyConfigurable(WeakSet.prototype, 'has');
