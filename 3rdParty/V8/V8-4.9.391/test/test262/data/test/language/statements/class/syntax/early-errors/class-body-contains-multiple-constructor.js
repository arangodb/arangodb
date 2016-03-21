// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 14.5.1
description: >
    ClassBody : ClassElementList

    It is a Syntax Error if PrototypePropertyNameList of ClassElementList contains more than one occurrence of "constructor".

negative: SyntaxError
---*/
class A {
  constructor() {}
  constructor() {}
}

