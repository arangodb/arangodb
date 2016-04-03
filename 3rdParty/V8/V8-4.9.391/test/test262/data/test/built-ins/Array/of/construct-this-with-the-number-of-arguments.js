// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.2.3
description: Passes the number of arguments to the constructor it calls.
info: >
  Array.of ( ...items )

  1. Let len be the actual number of arguments passed to this function.
  2. Let items be the List of arguments passed to this function.
  3. Let C be the this value.
  4. If IsConstructor(C) is true, then
    a. Let A be Construct(C, «len»).
  ...
---*/

var length;
var hits = 0;

function C(len) {
  length = len;
  hits++;
}

Array.of.call(C);
assert.sameValue(length, 0, '`Array.of.call(C);` called `new C(0)`');
assert.sameValue(hits, 1, 'Called constructor once per call');

Array.of.call(C, 'a', 'b')
assert.sameValue(length, 2, '`Array.of.call(C, "a", "b"));` called `new C(2)`');
assert.sameValue(hits, 2, 'Called constructor once per call');

Array.of.call(C, false, null, undefined);
assert.sameValue(
  length, 3,
  '`Array.of.call(C, false, null, undefined);` called `new C(3)`'
);
assert.sameValue(hits, 3, 'Called constructor once per call');