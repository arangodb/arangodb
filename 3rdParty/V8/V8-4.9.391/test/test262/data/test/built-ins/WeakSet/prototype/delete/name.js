// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.4.3.3
description: >
  WeakSet.prototype.delete.name value and writability.
info: >
  WeakSet.prototype.delete ( value )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  WeakSet.prototype.delete.name, 'delete',
  'The value of WeakSet.prototype.delete.name is "delete"'
);

verifyNotEnumerable(WeakSet.prototype.delete, 'name');
verifyNotWritable(WeakSet.prototype.delete, 'name');
verifyConfigurable(WeakSet.prototype.delete, 'name');
