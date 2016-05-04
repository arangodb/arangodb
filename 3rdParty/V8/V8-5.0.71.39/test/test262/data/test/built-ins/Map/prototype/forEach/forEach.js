// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.5
description: >
  Property type and descriptor.
info: >
  Map.prototype.forEach ( callbackfn [ , thisArg ] )

  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
---*/

assert.sameValue(
  typeof Map.prototype.forEach,
  'function',
  '`typeof Map.prototype.forEach` is `function`'
);

verifyNotEnumerable(Map.prototype, 'forEach');
verifyWritable(Map.prototype, 'forEach');
verifyConfigurable(Map.prototype, 'forEach');
