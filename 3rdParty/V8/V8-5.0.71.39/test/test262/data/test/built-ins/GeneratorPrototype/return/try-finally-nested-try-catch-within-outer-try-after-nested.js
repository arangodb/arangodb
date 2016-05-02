// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.3.1.3
description: >
    When a generator is paused within a `try` block of a `try..catch` statement
    and following a nested `try..catch` statment, `return` should interrupt
    control flow as if a `return` statement had appeared at that location in
    the function body.
---*/

var inCatch = false;
var inFinally = false;
function* g() {
  try {
    try {
      throw new Error();
    } catch (e) {
      inCatch = true;
    }
  } finally {
    inFinally = true;
  }
  yield;
  $ERROR('This code is unreachable');
}
var iter = g();
var result;

 iter.next();

assert.sameValue(inCatch, true, '`catch` code path executed');
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
