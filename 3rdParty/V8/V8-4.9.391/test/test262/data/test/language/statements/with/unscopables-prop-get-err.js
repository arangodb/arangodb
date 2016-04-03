// Copyright 2015 Mike Pennisi. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 8.1.1.2.1
description: >
    Behavior when accessing property of `Symbol.unscopables` property throws an error
info: >
    [...]
    6. If the withEnvironment flag of envRec is false, return true.
    7. Let unscopables be Get(bindings, @@unscopables).
    8. ReturnIfAbrupt(unscopables).
    9. If Type(unscopables) is Object, then
       a. Let blocked be ToBoolean(Get(unscopables, N)).
       b. ReturnIfAbrupt(blocked).

    ES6: 13.11.7 (The `with` Statement) Runtime Semantics: Evaluation
    [...]
    6. Set the withEnvironment flag of newEnv’s EnvironmentRecord to true.
    [...]
flags: [noStrict]
features: [Symbol.unscopables]
---*/

var env = { x: 86 };
env[Symbol.unscopables] = {};

Object.defineProperty(env[Symbol.unscopables], 'x', {
  get: function() {
    throw new Test262Error();
  }
});

with (env) {
  assert.throws(Test262Error, function() {
    void x;
  });
}
