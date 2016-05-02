// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.1
description: >
  Map.prototype.entries.name value and descriptor.
info: >
  Map.prototype.clear ( )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Map.prototype.clear.name, 'clear',
  'The value of `Map.prototype.clear.name` is `"clear"`'
);

verifyNotEnumerable(Map.prototype.clear, 'name');
verifyNotWritable(Map.prototype.clear, 'name');
verifyConfigurable(Map.prototype.clear, 'name');
