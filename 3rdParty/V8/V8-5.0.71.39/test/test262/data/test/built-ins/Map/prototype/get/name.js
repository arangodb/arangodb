// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.6
description: >
  Map.prototype.get.name value and descriptor.
info: >
  Map.prototype.get ( key )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Map.prototype.get.name, 'get',
  'The value of `Map.prototype.get.name` is `"get"`'
);

verifyNotEnumerable(Map.prototype.get, 'name');
verifyNotWritable(Map.prototype.get, 'name');
verifyConfigurable(Map.prototype.get, 'name');
