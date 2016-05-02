// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es7id: 14.1.19
description: If a computed property key contains a direct eval call, a new declarative environment is created.
info: >
  Runtime Semantics: IteratorBindingInitialization

  FormalParameter : BindingElement

  ...
  2. Let currentContext be the running execution context.
  3. Let originalEnv be the VariableEnvironment of currentContext.
  4. Assert: The VariableEnvironment and LexicalEnvironment of currentContext are the same.
  5. Assert: environment and originalEnv are the same.
  6. Let paramVarEnv be NewDeclarativeEnvironment(originalEnv).
  ...
---*/

var x = "outer";

function evalInComputedPropertyKey({[eval("var x = 'inner'")]: ignored}) {
  assert.sameValue(x, "outer");
}
evalInComputedPropertyKey({});
