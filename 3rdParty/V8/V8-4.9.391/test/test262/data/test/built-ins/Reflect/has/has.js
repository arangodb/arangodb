// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.9
description: >
  Reflect.has is configurable, writable and not enumerable.
info: >
  26.1.9 Reflect.has ( target, propertyKey )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

verifyNotEnumerable(Reflect, 'has');
verifyWritable(Reflect, 'has');
verifyConfigurable(Reflect, 'has');
