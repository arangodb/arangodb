// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.9
description: Property type and descriptor.
info: >
  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
---*/

assert.sameValue(
  typeof Array.prototype.findIndex,
  'function',
  '`typeof Array.prototype.findIndex` is `function`'
);

verifyNotEnumerable(Array.prototype, 'findIndex');
verifyWritable(Array.prototype, 'findIndex');
verifyConfigurable(Array.prototype, 'findIndex');
