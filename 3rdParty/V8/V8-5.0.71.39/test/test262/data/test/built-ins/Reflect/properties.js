// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1
description: >
  Reflect is configurable, writable and not enumerable.
info: >
  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
---*/

verifyNotEnumerable(this, 'Reflect');
verifyWritable(this, 'Reflect');
verifyConfigurable(this, 'Reflect');
