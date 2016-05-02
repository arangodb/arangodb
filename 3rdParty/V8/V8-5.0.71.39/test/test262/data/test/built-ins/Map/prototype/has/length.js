// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.7
description: >
  Map.prototype.has.length value and descriptor.
info: >
  Map.prototype.has ( key )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Map.prototype.has.length, 1,
  'The value of `Map.prototype.has.length` is `1`'
);

verifyNotEnumerable(Map.prototype.has, 'length');
verifyNotWritable(Map.prototype.has, 'length');
verifyConfigurable(Map.prototype.has, 'length');
