// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.9
description: >
  Reflect.has.name value and property descriptor
info: >
  26.1.9 Reflect.has ( target, propertyKey )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.has.name, 'has',
  'The value of `Reflect.has.name` is `"has"`'
);

verifyNotEnumerable(Reflect.has, 'name');
verifyNotWritable(Reflect.has, 'name');
verifyConfigurable(Reflect.has, 'name');
