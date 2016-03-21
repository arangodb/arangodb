// Copyright (C) Copyright 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 14.1.20
description: Assignment of function `name` attribute
info: >
    FunctionExpression : function ( FormalParameters ) { FunctionBody }

    1. If the function code for FunctionExpression is strict mode code, let
       strict be true. Otherwise let strict be false.
    2. Let scope be the LexicalEnvironment of the running execution context.
    3. Let closure be FunctionCreate(Normal, FormalParameters, FunctionBody,
       scope, strict).
    4. Perform MakeConstructor(closure).
    5. Return closure.

    FunctionExpression :
        function BindingIdentifier ( FormalParameters ) { FunctionBody }

    [...]
    5. Let name be StringValue of BindingIdentifier.
    [...]
    9. Perform SetFunctionName(closure, name).
includes: [propertyHelper.js]
---*/

assert.sameValue(Object.hasOwnProperty.call(function() {}, 'name'), false);

assert.sameValue(function func() {}.name, 'func');
verifyNotEnumerable(function func() {}, 'name');
verifyNotWritable(function func() {}, 'name');
verifyConfigurable(function func() {}, 'name');
