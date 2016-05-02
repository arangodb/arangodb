// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    The assignment target should obey `const` semantics.
es6id: 12.14.5.4
features: [const]
---*/

const c = 1;

assert.throws(TypeError, function() {
  ({ a: c } = { a: 2 });
});
