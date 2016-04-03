// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.2
description: Map.prototype.constructor value and descriptor
info: >
  The initial value of Map.prototype.constructor is the intrinsic object %Map%.
includes: [propertyHelper.js]
---*/

assert.sameValue(Map.prototype.constructor, Map);
assert.sameValue((new Map()).constructor, Map);

verifyNotEnumerable(Map.prototype, 'constructor');
verifyWritable(Map.prototype, 'constructor');
verifyConfigurable(Map.prototype, 'constructor');
