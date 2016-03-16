// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 21.1.3.18
description: >
  String.prototype.startsWith.length value and descriptor.
info: >
  21.1.3.18 String.prototype.startsWith ( searchString [ , position ] )

  The length property of the startsWith method is 1.

includes: [propertyHelper.js]
---*/

assert.sameValue(
  String.prototype.startsWith.length, 1,
  'The value of `String.prototype.startsWith.length` is `1`'
);

verifyNotEnumerable(String.prototype.startsWith, 'length');
verifyNotWritable(String.prototype.startsWith, 'length');
verifyConfigurable(String.prototype.startsWith, 'length');
