// Copyright (C) Copyright 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 14.4.13
description: >
    Assignment of function `name` attribute (GeneratorMethod)
info: >
    GeneratorMethod :
        * PropertyName ( StrictFormalParameters ) { GeneratorBody }

    [...]
    9. Perform SetFunctionName(closure, propKey).
includes: [propertyHelper.js]
features: [generators, Symbol]
---*/

var namedSym = Symbol('test262');
var anonSym = Symbol();

class A {
  *id() {}
  *[anonSym]() {}
  *[namedSym]() {}
  static *id() {}
  static *[anonSym]() {}
  static *[namedSym]() {}
}

assert.sameValue(A.prototype.id.name, 'id', 'via IdentifierName');
verifyNotEnumerable(A.prototype.id, 'name');
verifyNotWritable(A.prototype.id, 'name');
verifyConfigurable(A.prototype.id, 'name');

assert.sameValue(A.prototype[anonSym].name, '', 'via anonymous Symbol');
verifyNotEnumerable(A.prototype[anonSym], 'name');
verifyNotWritable(A.prototype[anonSym], 'name');
verifyConfigurable(A.prototype[anonSym], 'name');

assert.sameValue(A.prototype[namedSym].name, '[test262]', 'via Symbol');
verifyNotEnumerable(A.prototype[namedSym], 'name');
verifyNotWritable(A.prototype[namedSym], 'name');
verifyConfigurable(A.prototype[namedSym], 'name');

assert.sameValue(A.id.name, 'id', 'static via IdentifierName');
verifyNotEnumerable(A.id, 'name');
verifyNotWritable(A.id, 'name');
verifyConfigurable(A.id, 'name');

assert.sameValue(A[anonSym].name, '', 'static via anonymous Symbol');
verifyNotEnumerable(A[anonSym], 'name');
verifyNotWritable(A[anonSym], 'name');
verifyConfigurable(A[anonSym], 'name');

assert.sameValue(A[namedSym].name, '[test262]', 'static via Symbol');
verifyNotEnumerable(A[namedSym], 'name');
verifyNotWritable(A[namedSym], 'name');
verifyConfigurable(A[namedSym], 'name');
