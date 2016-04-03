// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.9
description: >
  Reflect.has.length value and property descriptor
includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.has.length, 2,
  'The value of `Reflect.has.length` is `2`'
);

verifyNotEnumerable(Reflect.has, 'length');
verifyNotWritable(Reflect.has, 'length');
verifyConfigurable(Reflect.has, 'length');
