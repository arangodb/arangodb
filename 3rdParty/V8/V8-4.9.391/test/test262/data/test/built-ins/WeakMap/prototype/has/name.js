// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.4
description: >
  WeakMap.prototype.has.name value and writability.
info: >
  WeakMap.prototype.has ( value )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  WeakMap.prototype.has.name, 'has',
  'The value of WeakMap.prototype.has.name is "has"'
);

verifyNotEnumerable(WeakMap.prototype.has, 'name');
verifyNotWritable(WeakMap.prototype.has, 'name');
verifyConfigurable(WeakMap.prototype.has, 'name');
