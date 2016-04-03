// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.1.1
description: Map.name value and descriptor.
info: >
  Map ( [ iterable ] )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(Map.name, 'Map', 'The value of Map.name is "Map"');

verifyNotEnumerable(Map, 'name');
verifyNotWritable(Map, 'name');
verifyConfigurable(Map, 'name');
