// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.1
description: >
  WeakMap.prototype.constructor value and property descriptor
info: >
  The initial value of WeakMap.prototype.constructor is the %WeakMap%
  intrinsic object.

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(WeakMap.prototype.constructor, WeakMap);
assert.sameValue((new WeakMap()).constructor, WeakMap);

verifyNotEnumerable(WeakMap.prototype, 'constructor');
verifyWritable(WeakMap.prototype, 'constructor');
verifyConfigurable(WeakMap.prototype, 'constructor');
