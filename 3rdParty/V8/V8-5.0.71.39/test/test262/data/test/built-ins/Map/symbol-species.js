// Copyright 2015 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
 Map has a property at `Symbol.species`
es6id: 23.1.2.2
author: Sam Mikes
description: Map[Symbol.species] exists per spec
includes:
  - propertyHelper.js
---*/

assert.sameValue(Map[Symbol.species], Map, "Map[Symbol.species] is Map");

verifyNotWritable(Map, Symbol.species, Symbol.species);
verifyNotEnumerable(Map, Symbol.species);
verifyConfigurable(Map, Symbol.species);
