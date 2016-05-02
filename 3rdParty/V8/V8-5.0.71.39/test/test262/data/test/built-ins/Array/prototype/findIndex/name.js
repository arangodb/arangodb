// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.9
description: >
  Array.prototype.findIndex.name value and descriptor.
info: >
  22.1.3.9 Array.prototype.findIndex ( predicate [ , thisArg ] )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Array.prototype.findIndex.name, 'findIndex',
  'The value of `Array.prototype.findIndex.name` is `"findIndex"`'
);

verifyNotEnumerable(Array.prototype.findIndex, 'name');
verifyNotWritable(Array.prototype.findIndex, 'name');
verifyConfigurable(Array.prototype.findIndex, 'name');
