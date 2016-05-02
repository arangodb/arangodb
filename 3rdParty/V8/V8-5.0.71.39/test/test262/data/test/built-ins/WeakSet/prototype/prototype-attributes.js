// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.4.2.1
description: >
  WeakSet.prototype is not writable, not enumerable and not configurable.
includes: [propertyHelper.js]
---*/

verifyNotEnumerable(WeakSet, 'prototype');
verifyNotWritable(WeakSet, 'prototype');
verifyNotConfigurable(WeakSet, 'prototype');
