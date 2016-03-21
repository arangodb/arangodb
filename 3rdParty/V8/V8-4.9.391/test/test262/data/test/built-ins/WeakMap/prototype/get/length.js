// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.3
description: >
  WeakMap.prototype.get.length value and descriptor.
info: >
  WeakMap.prototype.get ( key )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  WeakMap.prototype.get.length, 1,
  'The value of `WeakMap.prototype.get.length` is `1`'
);

verifyNotEnumerable(WeakMap.prototype.get, 'length');
verifyNotWritable(WeakMap.prototype.get, 'length');
verifyConfigurable(WeakMap.prototype.get, 'length');
