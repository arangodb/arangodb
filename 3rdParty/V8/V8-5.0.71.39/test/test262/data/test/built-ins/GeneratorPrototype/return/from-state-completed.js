// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.3.1.3
description: >
    Resuming abruptly from a generator in the 'completed' state should honor the
    abrupt completion and remain in the 'completed' state.
---*/

function* G() {}
var iter, result;

iter = G();
iter.next();

iter.return(33);

result = iter.next();

assert.sameValue(result.value, undefined, 'Result `value`');
assert.sameValue(result.done, true, 'Result `done` flag');
