// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    It is a Syntax Error if HasDirectSuper of MethodDefinition is true.
es6id: 12.2.5.1
negative: SyntaxError
---*/

({
  method() {
    super;
  }
});
