// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.4
description: >
  WeakMap.prototype.has property descriptor
info: >
  WeakMap.prototype.has ( value )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  typeof WeakMap.prototype.has,
  'function',
  'typeof WeakMap.prototype.has is "function"'
);

verifyNotEnumerable(WeakMap.prototype, 'has');
verifyWritable(WeakMap.prototype, 'has');
verifyConfigurable(WeakMap.prototype, 'has');
