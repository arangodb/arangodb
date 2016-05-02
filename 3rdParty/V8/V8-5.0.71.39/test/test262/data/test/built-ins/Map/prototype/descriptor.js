// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.2.1
description: Map.prototype property attributes.
info: >
  This property has the attributes { [[Writable]]: false, [[Enumerable]]: false,
  [[Configurable]]: false }.
includes: [propertyHelper.js]
---*/

verifyNotEnumerable(Map, 'prototype');
verifyNotWritable(Map, 'prototype');
verifyNotConfigurable(Map, 'prototype');
