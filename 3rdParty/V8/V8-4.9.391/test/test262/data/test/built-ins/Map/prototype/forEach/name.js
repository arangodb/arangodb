// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.5
description: >
  Map.prototype.forEach.name value and descriptor.
info: >
  Map.prototype.forEach ( callbackfn [ , thisArg ] )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Map.prototype.forEach.name, 'forEach',
  'The value of `Map.prototype.forEach.name` is `"forEach"`'
);

verifyNotEnumerable(Map.prototype.forEach, 'name');
verifyNotWritable(Map.prototype.forEach, 'name');
verifyConfigurable(Map.prototype.forEach, 'name');
