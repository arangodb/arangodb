// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.2
description: >
  WeakMap.prototype.delete property descriptor
info: >
  WeakMap.prototype.delete ( value )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  typeof WeakMap.prototype.delete,
  'function',
  'typeof WeakMap.prototype.delete is "function"'
);

verifyNotEnumerable(WeakMap.prototype, 'delete');
verifyWritable(WeakMap.prototype, 'delete');
verifyConfigurable(WeakMap.prototype, 'delete');
