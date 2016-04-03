// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.1
description: >
  Map.prototype.clear.length value and descriptor.
info: >
  Map.prototype.clear ( )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Map.prototype.clear.length, 0,
  'The value of `Map.prototype.clear.length` is `0`'
);

verifyNotEnumerable(Map.prototype.clear, 'length');
verifyNotWritable(Map.prototype.clear, 'length');
verifyConfigurable(Map.prototype.clear, 'length');
