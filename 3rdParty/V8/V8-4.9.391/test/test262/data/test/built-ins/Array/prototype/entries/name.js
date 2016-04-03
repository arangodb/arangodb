// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.4
description: >
  Array.prototype.entries.name value and descriptor.
info: >
  22.1.3.4 Array.prototype.entries ( )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Array.prototype.entries.name, 'entries',
  'The value of `Array.prototype.entries.name` is `"entries"`'
);

verifyNotEnumerable(Array.prototype.entries, 'name');
verifyNotWritable(Array.prototype.entries, 'name');
verifyConfigurable(Array.prototype.entries, 'name');
