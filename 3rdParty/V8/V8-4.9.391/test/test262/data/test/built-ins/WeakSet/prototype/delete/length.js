// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.4.3.3
description: >
  WeakSet.prototype.delete.length value and writability.
info: >
  WeakSet.prototype.delete ( value )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  WeakSet.prototype.delete.length, 1,
  'The value of WeakSet.prototype.delete.length is 1'
);

verifyNotEnumerable(WeakSet.prototype.delete, 'length');
verifyNotWritable(WeakSet.prototype.delete, 'length');
verifyConfigurable(WeakSet.prototype.delete, 'length');
