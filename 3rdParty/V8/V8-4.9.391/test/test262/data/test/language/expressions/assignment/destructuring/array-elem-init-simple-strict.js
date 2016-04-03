// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    It is a Syntax Error if LeftHandSideExpression is neither an
    ObjectLiteral nor an ArrayLiteral and
    IsValidSimpleAssignmentTarget(LeftHandSideExpression) is
    false.
es6id: 12.14.5.1
flags: [onlyStrict]
negative: SyntaxError
---*/

([arguments] = []);
