// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 14.5.1
description: >
    ClassElement : static MethodDefinition

    It is a Syntax Error if PropName of MethodDefinition is "prototype".

    (set)

negative: SyntaxError
---*/
class A {
  static set prototype() {}
}

