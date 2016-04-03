// Copyright 2015 Mike Pennisi. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 8.1.1.2.1
description: >
    `Symbol.unscopables` is not referenced when environment record does not have requested property
info: >
    1. Let envRec be the object Environment Record for which the method was
       invoked.
    2. Let bindings be the binding object for envRec.
    3. Let foundBinding be HasProperty(bindings, N)
    4. ReturnIfAbrupt(foundBinding).
    5. If foundBinding is false, return false.

    ES6: 13.11.7 (The `with` Statement) Runtime Semantics: Evaluation
    [...]
    6. Set the withEnvironment flag of newEnv’s EnvironmentRecord to true.
    [...]
flags: [noStrict]
features: [Symbol.unscopables]
---*/

var x = 0;
var env = {};
var callCount = 0;
Object.defineProperty(env, Symbol.unscopables, {
  get: function() {
    callCount += 1;
  }
});

with (env) {
  void x;
}

assert.sameValue(callCount, 0);
