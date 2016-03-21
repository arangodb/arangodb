// Copyright 2015 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
 Array has a property at `Symbol.species`
es6id: 22.1.2.5
author: Sam Mikes
description: Array[Symbol.species] exists per spec
includes:
  - propertyHelper.js
---*/

assert.sameValue(Array[Symbol.species], Array, "Array[Symbol.species] is Array");

verifyNotWritable(Array, Symbol.species, Symbol.species);
verifyNotEnumerable(Array, Symbol.species);
verifyConfigurable(Array, Symbol.species);
