// Copyright 2015 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
 ArrayBuffer has a property at `Symbol.species`
es6id: 24.1.3.3
author: Sam Mikes
description: ArrayBuffer[Symbol.species] exists per spec
features: [ ArrayBuffer, Symbol.species ]
includes:
  - propertyHelper.js
---*/

assert.sameValue(ArrayBuffer[Symbol.species], ArrayBuffer, "ArrayBuffer[Symbol.species] is ArrayBuffer");

assert.sameValue(Object.getOwnPropertyDescriptor(ArrayBuffer, Symbol.species).get.name,
                 "get [Symbol.species]");

verifyNotWritable(ArrayBuffer, Symbol.species, Symbol.species);
verifyNotEnumerable(ArrayBuffer, Symbol.species);
verifyConfigurable(ArrayBuffer, Symbol.species);
