// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.7
description: >
  Map.prototype.has.name value and descriptor.
info: >
  Map.prototype.has ( key )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Map.prototype.has.name, 'has',
  'The value of `Map.prototype.has.name` is `"has"`'
);

verifyNotEnumerable(Map.prototype.has, 'name');
verifyNotWritable(Map.prototype.has, 'name');
verifyConfigurable(Map.prototype.has, 'name');
