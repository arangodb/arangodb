// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.1
description: >
  Reflect.apply.name value and property descriptor
info: >
  26.1.1 Reflect.apply ( target, thisArgument, argumentsList )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.apply.name, 'apply',
  'The value of `Reflect.apply.name` is `"apply"`'
);

verifyNotEnumerable(Reflect.apply, 'name');
verifyNotWritable(Reflect.apply, 'name');
verifyConfigurable(Reflect.apply, 'name');
