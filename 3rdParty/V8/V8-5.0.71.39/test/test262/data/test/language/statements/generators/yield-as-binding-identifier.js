// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
  description: >
      `yield` is a reserved keyword within generator function bodies and may
      not be used as a binding identifier.
  es6id: 12.1.1
  negative: SyntaxError
 ---*/

var result;
function* g() {
  yield = 1;
}

result = g().next();
assert.sameValue(result.value, undefined);
assert.sameValue(result.done, true);
