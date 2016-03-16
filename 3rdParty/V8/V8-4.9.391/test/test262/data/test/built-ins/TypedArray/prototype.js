// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.2.2.3
description: >
  "prototype" property of TypedArray
info: >
  22.2.2.3 %TypedArray%.prototype

  This property has the attributes { [[Writable]]: false, [[Enumerable]]:
  false, [[Configurable]]: false }.
includes: [propertyHelper.js, testTypedArray.js]
---*/

verifyNotEnumerable(TypedArray, 'prototype');
verifyNotWritable(TypedArray, 'prototype');
verifyNotConfigurable(TypedArray, 'prototype');
