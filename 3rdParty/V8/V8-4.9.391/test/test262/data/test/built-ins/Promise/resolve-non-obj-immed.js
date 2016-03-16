// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: Resolving with a non-object value from within the executor function
es6id: 25.4.3.1
info: >
    [...]
    8. Let resolvingFunctions be CreateResolvingFunctions(promise).
    9. Let completion be Call(executor, undefined,
       «resolvingFunctions.[[Resolve]], resolvingFunctions.[[Reject]]»).

    25.4.1.3.2 Promise Resolve Functions
    7. If Type(resolution) is not Object, then
       a. Return FulfillPromise(promise, resolution).
---*/

var promise = new Promise(function(resolve) {
  resolve(45);
});

promise.then(function(value) {
    if (value !== 45) {
      $DONE('The promise should be fulfilled with the provided value.');
      return;
    }

    $DONE();
  }, function() {
    $DONE('The promise should not be rejected.');
  });
