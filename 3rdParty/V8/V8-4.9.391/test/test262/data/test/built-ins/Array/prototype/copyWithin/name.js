// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.3
description: >
  Array.prototype.copyWithin.name value and descriptor.
info: >
  22.1.3.3 Array.prototype.copyWithin (target, start [ , end ] )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Array.prototype.copyWithin.name, 'copyWithin',
  'The value of `Array.prototype.copyWithin.name` is `"copyWithin"`'
);

verifyNotEnumerable(Array.prototype.copyWithin, 'name');
verifyNotWritable(Array.prototype.copyWithin, 'name');
verifyConfigurable(Array.prototype.copyWithin, 'name');
