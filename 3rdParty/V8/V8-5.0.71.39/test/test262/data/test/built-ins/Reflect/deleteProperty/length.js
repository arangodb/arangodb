// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.4
description: >
  Reflect.deleteProperty.length value and property descriptor
includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.deleteProperty.length, 2,
  'The value of `Reflect.deleteProperty.length` is `2`'
);

verifyNotEnumerable(Reflect.deleteProperty, 'length');
verifyNotWritable(Reflect.deleteProperty, 'length');
verifyConfigurable(Reflect.deleteProperty, 'length');
