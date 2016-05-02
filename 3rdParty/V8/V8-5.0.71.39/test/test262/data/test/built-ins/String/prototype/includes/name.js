// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 21.1.3.7
description: >
  String.prototype.includes.name value and descriptor.
info: >
  21.1.3.7 String.prototype.includes ( searchString [ , position ] )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  String.prototype.includes.name, 'includes',
  'The value of `String.prototype.includes.name` is `"includes"`'
);

verifyNotEnumerable(String.prototype.includes, 'name');
verifyNotWritable(String.prototype.includes, 'name');
verifyConfigurable(String.prototype.includes, 'name');
