// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.4
description: >
  Array.prototype.entries.length value and descriptor.
info: >
  22.1.3.4 Array.prototype.entries ( )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Array.prototype.entries.length, 0,
  'The value of `Array.prototype.entries.length` is `0`'
);

verifyNotEnumerable(Array.prototype.entries, 'length');
verifyNotWritable(Array.prototype.entries, 'length');
verifyConfigurable(Array.prototype.entries, 'length');
