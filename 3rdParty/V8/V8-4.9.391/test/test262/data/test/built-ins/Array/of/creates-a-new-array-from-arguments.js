// Copyright (c) 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/*---
es6id: 22.1.2.3
description: >
  Array.of method creates a new Array with a variable number of arguments.
info: >
  22.1.2.3 Array.of ( ...items )

  ...
  7. Let k be 0.
  8. Repeat, while k < len
    a. Let kValue be items[k].
    b. Let Pk be ToString(k).
    c. Let defineStatus be CreateDataPropertyOrThrow(A,Pk, kValue).
    d. ReturnIfAbrupt(defineStatus).
    e. Increase k by 1.
  9. Let setStatus be Set(A, "length", len, true).
  10. ReturnIfAbrupt(setStatus).
  11. Return A.
---*/

var a1 = Array.of('Mike', 'Rick', 'Leo');
assert.sameValue(
  a1.length, 3,
  'The new array length is the same as the arguments size'
);
assert.sameValue(a1[0], 'Mike', 'set each property in order - #1');
assert.sameValue(a1[1], 'Rick', 'set each property in order - #2');
assert.sameValue(a1[2], 'Leo', 'set each property in order - #3');

var a2 = Array.of(undefined, false, null, undefined);
assert.sameValue(
  a2.length, 4,
  'Creates an array from the arguments, regarless of their type values'
);
assert.sameValue(a2[0], undefined, 'set each property in order - #1');
assert.sameValue(a2[1], false, 'set each property in order - #2');
assert.sameValue(a2[2], null, 'set each property in order - #3');
assert.sameValue(a2[3], undefined, 'set each property in order - #4');

var a3 = Array.of();
assert.sameValue(a3.length, 0, 'Array.of() returns an empty array');
