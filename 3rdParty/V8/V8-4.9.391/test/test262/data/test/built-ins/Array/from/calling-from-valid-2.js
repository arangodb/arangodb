// Copyright 2015 Microsoft Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
es6id: 22.1.2.1
description: Calling from with a valid map function with thisArg
info: >
  22.1.2.1 Array.from ( items [ , mapfn [ , thisArg ] ] )

  ...
  10. Let len be ToLength(Get(arrayLike, "length")).
  11. ReturnIfAbrupt(len).
  12. If IsConstructor(C) is true, then
    a. Let A be Construct(C, «len»).
  13. Else,
    b. Let A be ArrayCreate(len).
  14. ReturnIfAbrupt(A).
  15. Let k be 0.
  16. Repeat, while k < len
    a. Let Pk be ToString(k).
    b. Let kValue be Get(arrayLike, Pk).
    c. ReturnIfAbrupt(kValue).
    d. If mapping is true, then
      i. Let mappedValue be Call(mapfn, T, «kValue, k»).
  ...
---*/

var list = {
  '0': 41,
  '1': 42,
  '2': 43,
  length: 3
};
var calls = [];
var thisArg = {};

function mapFn (value) {
  calls.push({
    args: arguments,
    thisArg: this
  });
  return value * 2;
}

var result = Array.from(list, mapFn, thisArg);

assert.sameValue(result.length, 3, 'result.length');
assert.sameValue(result[0], 82, 'result[0]');
assert.sameValue(result[1], 84, 'result[1]');
assert.sameValue(result[2], 86, 'result[2]');

assert.sameValue(calls.length, 3, 'calls.length');

assert.sameValue(calls[0].args.length, 2, 'calls[0].args.length');
assert.sameValue(calls[0].args[0], 41, 'calls[0].args[0]');
assert.sameValue(calls[0].args[1], 0, 'calls[0].args[1]');
assert.sameValue(calls[0].thisArg, thisArg, 'calls[0].thisArg');

assert.sameValue(calls[1].args.length, 2, 'calls[1].args.length');
assert.sameValue(calls[1].args[0], 42, 'calls[1].args[0]');
assert.sameValue(calls[1].args[1], 1, 'calls[1].args[1]');
assert.sameValue(calls[1].thisArg, thisArg, 'calls[1].thisArg');

assert.sameValue(calls[2].args.length, 2, 'calls[2].args.length');
assert.sameValue(calls[2].args[0], 43, 'calls[2].args[0]');
assert.sameValue(calls[2].args[1], 2, 'calls[2].args[1]');
assert.sameValue(calls[2].thisArg, thisArg, 'calls[2].thisArg');
