// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.3
description: >
  Property type and descriptor.
info: >
  WeakMap.prototype.get ( key )

  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
---*/

assert.sameValue(
  typeof WeakMap.prototype.get,
  'function',
  '`typeof WeakMap.prototype.get` is `function`'
);

verifyNotEnumerable(WeakMap.prototype, 'get');
verifyWritable(WeakMap.prototype, 'get');
verifyConfigurable(WeakMap.prototype, 'get');
