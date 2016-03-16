// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Explicit iterator closing in response to error
es6id: 25.4.4.3
info: >
    11. Let result be PerformPromiseRace(iteratorRecord, promiseCapability, C).
    12. If result is an abrupt completion, then
        a. If iteratorRecord.[[done]] is false, let result be
           IteratorClose(iterator,result).
        b. IfAbruptRejectPromise(result, promiseCapability).


    25.4.4.3.1 Runtime Semantics: PerformPromiseRace

    1. Repeat
       [...]
       h. Let nextPromise be Invoke(C, "resolve", «nextValue»).
       i. ReturnIfAbrupt(nextPromise).
features: [Symbol.iterator]
---*/

var err = new Test262Error();
var iterDoneSpy = {};
var callCount = 0;
var CustomPromise = function(executor) {
  return new Promise(executor);
};
iterDoneSpy[Symbol.iterator] = function() {
  return {
    next: function() {
      return { value: null, done: false };
    },
    return: function() {
      callCount += 1;
    }
  };
};

CustomPromise.resolve = function() {
  throw err;
};

Promise.race.call(CustomPromise, iterDoneSpy)
  .then(function() {
    $ERROR('The promise should be rejected.');
  }, function(reason) {
    assert.sameValue(reason, err);
    $DONE();
  });

assert.sameValue(callCount, 1);
