// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.2.1
description: >
    Arguments of mapping function (traversed via iterator)
info: >
    [...]
    2. If mapfn is undefined, let mapping be false.
    3. else
       a. If IsCallable(mapfn) is false, throw a TypeError exception.
       b. If thisArg was supplied, let T be thisArg; else let T be undefined.
       c. Let mapping be true
    [...]
    6. If usingIterator is not undefined, then
       [...]
       g. Repeat
          [...]
          vii. If mapping is true, then
               1. Let mappedValue be Call(mapfn, T, «nextValue, k»).
               2. If mappedValue is an abrupt completion, return
                  IteratorClose(iterator, mappedValue).
               3. Let mappedValue be mappedValue.[[value]].
features: [Symbol.iterator]
---*/

var args = [];
var firstResult = { done: false, value: {} };
var secondResult = { done: false, value: {} };
var mapFn = function(value, idx) {
  args.push(arguments);
};
var items = {};
var nextResult = firstResult;
var nextNextResult = secondResult;

items[Symbol.iterator] = function() {
  return {
    next: function() {
      var result = nextResult;
      nextResult = nextNextResult;
      nextNextResult = { done: true };

      return result;
    }
  };
};

Array.from(items, mapFn);

assert.sameValue(args.length, 2, 'Iteration count');

assert.sameValue(args[0].length, 2, 'First iteration: arguments length');
assert.sameValue(
  args[0][0], firstResult.value, 'First iteration: first argument'
);
assert.sameValue(args[0][1], 0, 'First iteration: second argument');

assert.sameValue(args[1].length, 2, 'Second iteration: arguments length');
assert.sameValue(
  args[1][0], secondResult.value, 'Second iteration: first argument'
);
assert.sameValue(args[1][1], 1, 'Second iteration: second argument');
