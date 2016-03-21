// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 14.1.2
description: >
  It is a SyntaxError if a a not simple parameters list contains duplicate names
info: >
  FormalParameters : FormalParameterList

  It is a Syntax Error if IsSimpleParameterList of FormalParameterList is false
  and BoundNames of FormalParameterList contains any duplicate elements.
flags: [noStrict]
negative: SyntaxError
---*/

var fn = function (a, a = 1) {};
