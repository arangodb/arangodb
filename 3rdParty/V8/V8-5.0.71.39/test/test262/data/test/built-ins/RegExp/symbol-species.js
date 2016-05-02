// Copyright 2015 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
 RegExp has a property at `Symbol.species`
es6id: 21.2.4.2
author: Sam Mikes
description: RegExp[Symbol.species] exists per spec
includes:
  - propertyHelper.js
---*/

assert.sameValue(RegExp[Symbol.species], RegExp, "RegExp[Symbol.species] is RegExp");

verifyNotWritable(RegExp, Symbol.species, Symbol.species);
verifyNotEnumerable(RegExp, Symbol.species);
verifyConfigurable(RegExp, Symbol.species);
