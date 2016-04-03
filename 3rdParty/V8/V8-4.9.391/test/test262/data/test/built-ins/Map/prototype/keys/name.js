// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.8
description: >
  Map.prototype.keys.name value and descriptor.
info: >
  Map.prototype.keys ()

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Map.prototype.keys.name, 'keys',
  'The value of `Map.prototype.keys.name` is `"keys"`'
);

verifyNotEnumerable(Map.prototype.keys, 'name');
verifyNotWritable(Map.prototype.keys, 'name');
verifyConfigurable(Map.prototype.keys, 'name');
