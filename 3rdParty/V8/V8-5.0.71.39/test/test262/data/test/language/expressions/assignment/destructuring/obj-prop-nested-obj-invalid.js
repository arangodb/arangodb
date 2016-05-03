// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    It is a Syntax Error if LeftHandSideExpression is either
    an ObjectLiteral or an ArrayLiteral and if the lexical
    token sequence matched by LeftHandSideExpression cannot be
    parsed with no tokens left over using AssignmentPattern as
    the goal symbol.
es6id: 12.14.5.1
negative: SyntaxError
---*/

({ x: { get x() {} } } = { x: {} });
