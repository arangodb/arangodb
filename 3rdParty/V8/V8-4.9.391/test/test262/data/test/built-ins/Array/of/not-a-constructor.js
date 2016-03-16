// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.2.3
description: >
  Array.of is not a constructor.
---*/

assert.throws(TypeError, function() {
  new Array.of();
});
