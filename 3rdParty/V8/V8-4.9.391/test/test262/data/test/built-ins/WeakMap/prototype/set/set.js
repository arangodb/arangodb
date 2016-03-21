// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.5
description: WeakMap.prototype.set property descriptor
info: >
  WeakMap.prototype.set ( key, value )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  typeof WeakMap.prototype.set,
  'function',
  'typeof WeakMap.prototype.set is "function"'
);

verifyNotEnumerable(WeakMap.prototype, 'set');
verifyWritable(WeakMap.prototype, 'set');
verifyConfigurable(WeakMap.prototype, 'set');
