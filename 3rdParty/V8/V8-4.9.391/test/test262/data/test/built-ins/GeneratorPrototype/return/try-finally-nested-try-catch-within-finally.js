// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.3.1.3
description: >
    When a generator is paused within a `finally` block of a `try..catch`
    statement, `return` should interrupt control flow as if a `return`
    statement had appeared at that location in the function body.
---*/

var inFinally = false;
function* g() {
  try {
    throw new Error();
    try {
    } catch (e) {}
  } finally {
    inFinally = true;
    yield;
    $ERROR('This code is unreachable (within `finally` block)');
  }
  $ERROR('This code is unreachable (following outer `try` statement)');
}
var iter = g();
var result;

result = iter.next();

assert.sameValue(inFinally, true, '`finally` code path executed');

result = iter.return(45);
assert.sameValue(result.value, 45, 'Result `value` following `return`');
assert.sameValue(result.done, true, 'Result `done` flag following `return`');

result = iter.next();
assert.sameValue(
  result.value, undefined, 'Result `value` is undefined when complete'
);
assert.sameValue(
  result.done, true, 'Result `done` flag is `true` when complete'
);
