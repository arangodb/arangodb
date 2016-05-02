// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.9
description: >
  Map.prototype.set.name value and descriptor.
info: >
  Map.prototype.set ( key , value )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Map.prototype.set.name, 'set',
  'The value of `Map.prototype.set.name` is `"set"`'
);

verifyNotEnumerable(Map.prototype.set, 'name');
verifyNotWritable(Map.prototype.set, 'name');
verifyConfigurable(Map.prototype.set, 'name');
