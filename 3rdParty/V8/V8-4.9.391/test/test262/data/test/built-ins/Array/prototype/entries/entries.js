// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.4
description: >
  Property type and descriptor.
info: >
  22.1.3.4 Array.prototype.entries ( )

  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
---*/

assert.sameValue(
  typeof Array.prototype.entries,
  'function',
  '`typeof Array.prototype.entries` is `function`'
);

verifyNotEnumerable(Array.prototype, 'entries');
verifyWritable(Array.prototype, 'entries');
verifyConfigurable(Array.prototype, 'entries');
