// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.3
description: Property type and descriptor.
info: >
  22.1.3.3 Array.prototype.copyWithin (target, start [ , end ] )

  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
---*/

assert.sameValue(
  typeof Array.prototype.copyWithin,
  'function',
  '`typeof Array.prototype.copyWithin` is `function`'
);

verifyNotEnumerable(Array.prototype, 'copyWithin');
verifyWritable(Array.prototype, 'copyWithin');
verifyConfigurable(Array.prototype, 'copyWithin');
