// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
  description: >
      The initial value of the keys property is the same function object as the
      initial value of the values property.
  es6id: 23.2.3.8
 ---*/

assert.sameValue(
 Set.prototype.keys,
 Set.prototype.values,
 "The value of `Set.prototype.keys` is `Set.prototype.values`"
);
