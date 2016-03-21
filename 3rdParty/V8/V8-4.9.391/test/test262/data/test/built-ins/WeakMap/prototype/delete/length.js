// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.2
description: >
  WeakMap.prototype.delete.length value and writability.
info: >
  WeakMap.prototype.delete ( value )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  WeakMap.prototype.delete.length, 1,
  'The value of WeakMap.prototype.delete.length is 1'
);

verifyNotEnumerable(WeakMap.prototype.delete, 'length');
verifyNotWritable(WeakMap.prototype.delete, 'length');
verifyConfigurable(WeakMap.prototype.delete, 'length');
