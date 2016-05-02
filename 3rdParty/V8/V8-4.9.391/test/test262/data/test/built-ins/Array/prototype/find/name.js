// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.8
description: >
  Array.prototype.find.name value and descriptor.
info: >
  22.1.3.8 Array.prototype.find ( predicate [ , thisArg ] )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Array.prototype.find.name, 'find',
  'The value of `Array.prototype.find.name` is `"find"`'
);

verifyNotEnumerable(Array.prototype.find, 'name');
verifyNotWritable(Array.prototype.find, 'name');
verifyConfigurable(Array.prototype.find, 'name');
