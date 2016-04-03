// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.4.3.1
description: WeakSet.prototype.add.length descriptor
info: >
  WeakSet.prototype.add ( value )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  WeakSet.prototype.add.length, 1,
  'The value of `WeakSet.prototype.add.length` is `1`'
);

verifyNotEnumerable(WeakSet.prototype.add, 'length');
verifyNotWritable(WeakSet.prototype.add, 'length');
verifyConfigurable(WeakSet.prototype.add, 'length');
