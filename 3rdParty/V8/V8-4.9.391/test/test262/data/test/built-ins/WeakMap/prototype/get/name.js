// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.3
description: >
  WeakMap.prototype.get.name value and descriptor.
info: >
  WeakMap.prototype.get ( key )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  WeakMap.prototype.get.name, 'get',
  'The value of `WeakMap.prototype.get.name` is `"get"`'
);

verifyNotEnumerable(WeakMap.prototype.get, 'name');
verifyNotWritable(WeakMap.prototype.get, 'name');
verifyConfigurable(WeakMap.prototype.get, 'name');
