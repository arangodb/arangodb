// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.4.3.1
description: WeakSet.prototype.add.name descriptor
info: >
  WeakSet.prototype.add ( value )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  WeakSet.prototype.add.name, 'add',
  'The value of WeakSet.prototype.add.name is "add"'
);

verifyNotEnumerable(WeakSet.prototype.add, 'name');
verifyNotWritable(WeakSet.prototype.add, 'name');
verifyConfigurable(WeakSet.prototype.add, 'name');
