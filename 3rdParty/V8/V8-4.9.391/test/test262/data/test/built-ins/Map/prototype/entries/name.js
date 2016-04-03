// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.4
description: >
  Map.prototype.entries.name value and descriptor.
info: >
  Map.prototype.entries ( )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Map.prototype.entries.name, 'entries',
  'The value of `Map.prototype.entries.name` is `"entries"`'
);

verifyNotEnumerable(Map.prototype.entries, 'name');
verifyNotWritable(Map.prototype.entries, 'name');
verifyConfigurable(Map.prototype.entries, 'name');
