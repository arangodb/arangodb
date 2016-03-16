// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    The assignment target should obey `const` semantics.
es6id: 12.14.5.3
features: [const]
---*/

const c = null;

assert.throws(TypeError, function() {
  [ ...c ] = [1];
});
