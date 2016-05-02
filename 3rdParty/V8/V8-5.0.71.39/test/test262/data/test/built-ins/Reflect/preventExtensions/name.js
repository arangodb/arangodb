// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.12
description: >
  Reflect.preventExtensions.name value and property descriptor
info: >
  26.1.12 Reflect.preventExtensions ( target )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.preventExtensions.name, 'preventExtensions',
  'The value of `Reflect.preventExtensions.name` is `"preventExtensions"`'
);

verifyNotEnumerable(Reflect.preventExtensions, 'name');
verifyNotWritable(Reflect.preventExtensions, 'name');
verifyConfigurable(Reflect.preventExtensions, 'name');
