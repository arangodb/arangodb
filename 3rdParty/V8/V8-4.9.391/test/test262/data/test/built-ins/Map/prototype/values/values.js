// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.11
description: >
  Property type and descriptor.
info: >
  Map.prototype.values ()

  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
---*/

assert.sameValue(
  typeof Map.prototype.values,
  'function',
  '`typeof Map.prototype.values` is `function`'
);

verifyNotEnumerable(Map.prototype, 'values');
verifyWritable(Map.prototype, 'values');
verifyConfigurable(Map.prototype, 'values');
