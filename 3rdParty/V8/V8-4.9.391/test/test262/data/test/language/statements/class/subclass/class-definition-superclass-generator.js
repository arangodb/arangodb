// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 14.5.14_S6.g.i
description: >
    Runtime Semantics: ClassDefinitionEvaluation

    If superclass has a [[FunctionKind]] internal slot whose value is "generator", throw a TypeError exception.
---*/
function * G() {}

assert.throws(TypeError, function() {
  class A extends G {
   constructor() {
      super();
    }
  }
});

