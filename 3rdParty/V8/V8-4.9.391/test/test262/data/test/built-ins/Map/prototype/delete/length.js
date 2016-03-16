// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.3
description: >
  Map.prototype.delete.length value and descriptor.
info: >
  Map.prototype.delete ( key )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Map.prototype.delete.length, 1,
  'The value of `Map.prototype.delete.length` is `1`'
);

verifyNotEnumerable(Map.prototype.delete, 'length');
verifyNotWritable(Map.prototype.delete, 'length');
verifyConfigurable(Map.prototype.delete, 'length');
