// Copyright (C) Copyright 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 14.3.9
description: Assignment of function `name` attribute ("get" accessor)
info: >
    MethodDefinition : get PropertyName ( ) { FunctionBody }

    [...]
    8. Perform SetFunctionName(closure, propKey, "get").
includes: [propertyHelper.js]
features: [Symbol]
---*/

var namedSym = Symbol('test262');
var anonSym = Symbol();
var o, getter;

o = {
  get id() {},
  get [anonSym]() {},
  get [namedSym]() {}
};

getter = Object.getOwnPropertyDescriptor(o, 'id').get;
assert.sameValue(getter.name, 'get id');
verifyNotEnumerable(getter, 'name');
verifyNotWritable(getter, 'name');
verifyConfigurable(getter, 'name');

getter = Object.getOwnPropertyDescriptor(o, anonSym).get;
assert.sameValue(getter.name, 'get ');
verifyNotEnumerable(getter, 'name');
verifyNotWritable(getter, 'name');
verifyConfigurable(getter, 'name');

getter = Object.getOwnPropertyDescriptor(o, namedSym).get;
assert.sameValue(getter.name, 'get [test262]');
verifyNotEnumerable(getter, 'name');
verifyNotWritable(getter, 'name');
verifyConfigurable(getter, 'name');
