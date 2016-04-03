// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.9
description: >
  Property type and descriptor.
info: >
  Map.prototype.set ( key , value )

  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
---*/

assert.sameValue(
  typeof Map.prototype.set,
  'function',
  '`typeof Map.prototype.set` is `function`'
);

verifyNotEnumerable(Map.prototype, 'set');
verifyWritable(Map.prototype, 'set');
verifyConfigurable(Map.prototype, 'set');
