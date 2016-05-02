// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.6
description: >
  Property type and descriptor.
info: >
  Map.prototype.get ( key )

  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
---*/

assert.sameValue(
  typeof Map.prototype.get,
  'function',
  '`typeof Map.prototype.get` is `function`'
);

verifyNotEnumerable(Map.prototype, 'get');
verifyWritable(Map.prototype, 'get');
verifyConfigurable(Map.prototype, 'get');
