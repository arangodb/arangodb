// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.1
description: >
  Reflect.apply.length value and property descriptor
includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.apply.length, 3,
  'The value of `Reflect.apply.length` is `3`'
);

verifyNotEnumerable(Reflect.apply, 'length');
verifyNotWritable(Reflect.apply, 'length');
verifyConfigurable(Reflect.apply, 'length');
