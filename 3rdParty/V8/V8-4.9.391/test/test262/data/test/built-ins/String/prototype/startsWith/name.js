// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 21.1.3.18
description: >
  String.prototype.startsWith.name value and descriptor.
info: >
  21.1.3.18 String.prototype.startsWith ( searchString [ , position ] )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  String.prototype.startsWith.name, 'startsWith',
  'The value of `String.prototype.startsWith.name` is `"startsWith"`'
);

verifyNotEnumerable(String.prototype.startsWith, 'name');
verifyNotWritable(String.prototype.startsWith, 'name');
verifyConfigurable(String.prototype.startsWith, 'name');
