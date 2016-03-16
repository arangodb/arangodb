// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.2.3.16
description: >
  "lastIndexOf" property of TypedArrayPrototype
info: >
  ES6 section 17: Every other data property described in clauses 18 through 26
  and in Annex B.2 has the attributes { [[Writable]]: true,
  [[Enumerable]]: false, [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js, testTypedArray.js]
---*/

var TypedArrayPrototype = TypedArray.prototype;

verifyNotEnumerable(TypedArrayPrototype, 'lastIndexOf');
verifyWritable(TypedArrayPrototype, 'lastIndexOf');
verifyConfigurable(TypedArrayPrototype, 'lastIndexOf');
