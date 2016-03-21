// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 13.3.3.7
description: >
  Throws a ReferenceError from a unsolvable reference as the default parameter.
---*/

function fn(a = unresolvableReference) {}

assert.throws(ReferenceError, function() {
  fn();
});
