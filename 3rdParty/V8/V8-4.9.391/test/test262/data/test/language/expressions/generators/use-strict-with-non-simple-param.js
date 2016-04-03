// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es7id: 14.4.1
description: >
  A SyntaxError is thrown if a generator contains a non-simple parameter list and a UseStrict directive.
info: >
  Static Semantics: Early Errors

  It is a Syntax Error if ContainsUseStrict of GeneratorBody is true and IsSimpleParameterList of FormalParameters is false.
negative: SyntaxError
---*/

var f = function*(a = 0) {
  "use strict";
}
