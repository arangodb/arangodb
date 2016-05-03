// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.4.3.1
description: WeakSet.prototype.add property descriptor
info: >
  WeakSet.prototype.add ( value )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  typeof WeakSet.prototype.add,
  'function',
  'typeof WeakSet.prototype.add is "function"'
);

verifyNotEnumerable(WeakSet.prototype, 'add');
verifyWritable(WeakSet.prototype, 'add');
verifyConfigurable(WeakSet.prototype, 'add');
