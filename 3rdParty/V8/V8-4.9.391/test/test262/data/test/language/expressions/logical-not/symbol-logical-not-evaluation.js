// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 12.5.12.1
description: >
    "Logical Not" coercion operation on Symbols
---*/
var sym = Symbol();

assert.sameValue(!sym, false, "`!sym` is `false`");
assert.sameValue(!!sym, true, "`!!sym` is `true`");

if (!sym) {
  $ERROR("ToBoolean(Symbol) always returns `true`");
} else if (sym) {
  assert(true, "`sym` evaluates to `true`");
} else {
  $ERROR("ToBoolean(Symbol) always returns `true`");
}
