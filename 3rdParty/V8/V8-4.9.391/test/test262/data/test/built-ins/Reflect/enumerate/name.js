// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.5
description: >
  Reflect.enumerate.name value and property descriptor
info: >
  26.1.5 Reflect.enumerate ( target )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.enumerate.name, 'enumerate',
  'The value of `Reflect.enumerate.name` is `"enumerate"`'
);

verifyNotEnumerable(Reflect.enumerate, 'name');
verifyNotWritable(Reflect.enumerate, 'name');
verifyConfigurable(Reflect.enumerate, 'name');
