// Copyright 2015 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
 Set has a property at `Symbol.species`
es6id: 23.2.2.2
author: Sam Mikes
description: Set[Symbol.species] exists per spec
includes:
  - propertyHelper.js
---*/

assert.sameValue(Set[Symbol.species], Set, "Set[Symbol.species] is Set");

verifyNotWritable(Set, Symbol.species, Symbol.species);
verifyNotEnumerable(Set, Symbol.species);
verifyConfigurable(Set, Symbol.species);
