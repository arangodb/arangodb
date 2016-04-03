// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 14.5.14
description: >
    Runtime Semantics: ClassDefinitionEvaluation

    If superclass is null, then
      Let protoParent be null.
      Let constructorParent be the intrinsic object %FunctionPrototype%.

    `extends null` requires return override in the constructor
---*/
class Foo extends null {
  constructor() {
    return {};
  }
}

var f = new Foo();

assert.sameValue(Object.getPrototypeOf(f), Object.prototype);
