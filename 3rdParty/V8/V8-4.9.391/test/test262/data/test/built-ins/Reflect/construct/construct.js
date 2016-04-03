// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.2
description: >
  Reflect.construct is configurable, writable and not enumerable.
info: >
  26.1.2 Reflect.construct ( target, argumentsList [, newTarget] )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

verifyNotEnumerable(Reflect, 'construct');
verifyWritable(Reflect, 'construct');
verifyConfigurable(Reflect, 'construct');
