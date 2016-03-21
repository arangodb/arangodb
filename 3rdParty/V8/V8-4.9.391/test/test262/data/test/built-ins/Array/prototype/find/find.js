// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.8
description: Property type and descriptor.
info: >
  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
---*/

assert.sameValue(
  typeof Array.prototype.find,
  'function',
  '`typeof Array.prototype.find` is `function`'
);

verifyNotEnumerable(Array.prototype, 'find');
verifyWritable(Array.prototype, 'find');
verifyConfigurable(Array.prototype, 'find');
