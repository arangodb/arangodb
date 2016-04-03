// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.2.12
description: >
  Vars whose names are in the parameters list, initially have the same value as
  the corresponding initialized parameter.
info: >
  9.2.12 FunctionDeclarationInstantiation(func, argumentsList)

  ...
  27. If hasParameterExpressions is false, then
    ...
  28. Else,
    a. NOTE A separate Environment Record is needed to ensure that closures
    created by expressions in the formal parameter list do not have visibility
    of declarations in the function body.
    b. Let varEnv be NewDeclarativeEnvironment(env).
    c. Let varEnvRec be varEnv’s EnvironmentRecord.
    d. Set the VariableEnvironment of calleeContext to varEnv.
    e. Let instantiatedVarNames be a new empty List.
    f. For each n in varNames, do
      i. If n is not an element of instantiatedVarNames, then
        1. Append n to instantiatedVarNames.
        2. Let status be varEnvRec.CreateMutableBinding(n).
        3. Assert: status is never an abrupt completion.
        4. If n is not an element of parameterNames or if n is an element of
        functionNames, let initialValue be undefined.
        5. else,
          a. Let initialValue be envRec.GetBindingValue(n, false).
    ...
---*/

function fn(a = 1) {
  var a;

  return a;
}

assert.sameValue(fn(), 1);
