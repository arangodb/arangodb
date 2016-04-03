// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.3.1.3
description: >
    When a generator is paused after a `try..finally` statement, `return`
    should interrupt control flow as if a `return` statement had appeared at
    that location in the function body.
---*/

var afterFinally = false;
function* g() {
  try {
  } finally {}
  afterFinally = true;
  yield;
}
var iter = g();
var result;

iter.next();

assert.sameValue(afterFinally, true);

result = iter.return(45);
assert.sameValue(result.value, 45, 'Result `value` following `return`');
assert.sameValue(result.done, true, 'Result `done` flag following `return`');

result = iter.next();
assert.sameValue(
  result.value, undefined, 'Result `value` is undefined when done'
);
assert.sameValue(result.done, true, 'Result `done` flag is `true` when done');
