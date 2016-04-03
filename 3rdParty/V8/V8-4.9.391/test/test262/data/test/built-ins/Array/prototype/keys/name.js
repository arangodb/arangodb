// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.13
description: >
  Array.prototype.keys.name value and descriptor.
info: >
  22.1.3.13 Array.prototype.keys ( )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Array.prototype.keys.name, 'keys',
  'The value of `Array.prototype.keys.name` is `"keys"`'
);

verifyNotEnumerable(Array.prototype.keys, 'name');
verifyNotWritable(Array.prototype.keys, 'name');
verifyConfigurable(Array.prototype.keys, 'name');
