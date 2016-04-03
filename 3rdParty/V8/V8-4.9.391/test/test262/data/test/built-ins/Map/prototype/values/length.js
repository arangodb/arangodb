// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.11
description: >
  Map.prototype.values.length value and descriptor.
info: >
  Map.prototype.values ()

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Map.prototype.values.length, 0,
  'The value of `Map.prototype.values.length` is `0`'
);

verifyNotEnumerable(Map.prototype.values, 'length');
verifyNotWritable(Map.prototype.values, 'length');
verifyConfigurable(Map.prototype.values, 'length');
