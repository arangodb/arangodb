// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.5
description: >
  Reflect.enumerate.length value and property descriptor
includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.enumerate.length, 1,
  'The value of `Reflect.enumerate.length` is `1`'
);

verifyNotEnumerable(Reflect.enumerate, 'length');
verifyNotWritable(Reflect.enumerate, 'length');
verifyConfigurable(Reflect.enumerate, 'length');
