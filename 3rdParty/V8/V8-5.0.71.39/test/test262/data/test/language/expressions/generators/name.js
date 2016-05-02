// Copyright (C) Copyright 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 14.4.1
description: Assignment of function `name` attribute
info: >
    GeneratorExpression : function * ( FormalParameters ) { GeneratorBody }

    1. If the function code for this GeneratorExpression is strict mode code,
       let strict be true. Otherwise let strict be false.
    2. Let scope be the LexicalEnvironment of the running execution context.
    3. Let closure be GeneratorFunctionCreate(Normal, FormalParameters,
       GeneratorBody, scope, strict).
    4. Let prototype be ObjectCreate(%GeneratorPrototype%).
    5. Perform MakeConstructor(closure, true, prototype).
    6. Return closure.

    GeneratorExpression : function * BindingIdentifier ( FormalParameters ) { GeneratorBody }

    [...]
    10. Perform SetFunctionName(closure, name).
includes: [propertyHelper.js]
---*/

assert.sameValue(Object.hasOwnProperty.call(function*() {}, 'name'), false);

assert.sameValue(function* func() {}.name, 'func');
verifyNotEnumerable(function* func() {}, 'name');
verifyNotWritable(function* func() {}, 'name');
verifyConfigurable(function* func() {}, 'name');
